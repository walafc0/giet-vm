///////////////////////////////////////////////////////////////////////////////////////
// File   : main.c   (for transpose application)
// Date   : february 2014
// author : Alain Greiner
///////////////////////////////////////////////////////////////////////////////////////
// This multi-threaded application makes a transpose for a NN*NN pixels image.
// It can run on a multi-processors, multi-clusters architecture, with one thread
// per processor. 
//
// The image is read from a file (one byte per pixel), transposed and
// saved in a second file. Then the transposed image is read from the second file,
// transposed again and saved in a third file.
//
// The input and output buffers containing the image are distributed in all clusters.
//
// - The image size NN must fit the frame buffer size.
// - The block size in block device must be 512 bytes.
// - The number of clusters  must be a power of 2 no larger than 64.
// - The number of processors per cluster must be a power of 2 no larger than 4.
//
// For each image the application makes a self test (checksum for each line).
// The actual display on the frame buffer depends on frame buffer availability.
///////////////////////////////////////////////////////////////////////////////////////

#include "stdio.h"
#include "user_barrier.h"
#include "malloc.h"

#define BLOCK_SIZE            512                         // block size on disk
#define X_MAX                 8                           // max number of clusters in row
#define Y_MAX                 8                           // max number of clusters in column
#define PROCS_MAX             4                           // max number of procs per cluster
#define CLUSTER_MAX           (X_MAX * Y_MAX)             // max number of clusters
#define NN                    256                         // image size : nlines = npixels
#define INITIAL_FILE_PATH     "/misc/lena_256.raw"        // pathname on virtual disk
#define TRANSPOSED_FILE_PATH  "/home/lena_transposed.raw" // pathname on virtual disk
#define RESTORED_FILE_PATH    "/home/lena_restored.raw"   // pathname on virtual disk
#define INSTRUMENTATION_OK    1                           // display statistics on TTY 

// macro to use a shared TTY
#define printf(...)     lock_acquire( &tty_lock ); \
                        giet_tty_printf(__VA_ARGS__);  \
                        lock_release( &tty_lock )

///////////////////////////////////////////////////////
// global variables stored in seg_data in cluster(0,0)
///////////////////////////////////////////////////////

// instrumentation counters for each processor in each cluster 
unsigned int LOAD_START[X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};
unsigned int LOAD_END  [X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};
unsigned int TRSP_START[X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};
unsigned int TRSP_END  [X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};
unsigned int DISP_START[X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};
unsigned int DISP_END  [X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};
unsigned int STOR_START[X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};
unsigned int STOR_END  [X_MAX][Y_MAX][PROCS_MAX] = {{{ 0 }}};

// arrays of pointers on distributed buffers
// one input buffer & one output buffer per cluster
unsigned char*  buf_in [CLUSTER_MAX];
unsigned char*  buf_out[CLUSTER_MAX];

// checksum variables 
unsigned check_line_before[NN];
unsigned check_line_after[NN];

// lock protecting shared TTY
user_lock_t  tty_lock;

// global & local synchronisation variables
giet_sqt_barrier_t barrier;

volatile unsigned int global_init_ok = 0;
volatile unsigned int local_init_ok[X_MAX][Y_MAX] = {{ 0 }};

//////////////////////////////////////////
__attribute__ ((constructor)) void main()
//////////////////////////////////////////
{
    unsigned int l;                  // line index for loops
    unsigned int p;                  // pixel index for loops

    // processor identifiers
    unsigned int x;                  // x cluster coordinate
    unsigned int y;                  // y cluster coordinate
    unsigned int lpid;               // local processor index

    // plat-form parameters
    unsigned int x_size;             // number of clusters in a row
    unsigned int y_size;             // number of clusters in a column
    unsigned int nprocs;             // number of processors per cluster
    
    giet_proc_xyp( &x, &y, &lpid);             

    giet_procs_number( &x_size , &y_size , &nprocs );

    unsigned int nclusters     = x_size * y_size;               // number of clusters
    unsigned int ntasks        = x_size * y_size * nprocs;      // number of tasks
    unsigned int npixels       = NN * NN;                       // pixels per image
    unsigned int iteration     = 0;                             // iiteration iter
    int          fd_initial    = 0;                             // initial file descriptor
    int          fd_transposed = 0;                             // transposed file descriptor
    int          fd_restored   = 0;                             // restored file descriptor
    unsigned int cluster_id    = (x * y_size) + y;              // "continuous" index   
    unsigned int task_id       = (cluster_id * nprocs) + lpid;  // "continuous" task index

    // checking parameters
    giet_assert( ((nprocs == 1) || (nprocs == 2) || (nprocs == 4)),
                 "[TRANSPOSE ERROR] number of procs per cluster must be 1, 2 or 4");

    giet_assert( ((x_size == 1) || (x_size == 2) || (x_size == 4) || 
                  (x_size == 8) || (x_size == 16)),
                 "[TRANSPOSE ERROR] x_size must be 1,2,4,8,16");

    giet_assert( ((y_size == 1) || (y_size == 2) || (y_size == 4) || 
                  (y_size == 8) || (y_size == 16)),
                 "[TRANSPOSE ERROR] y_size must be 1,2,4,8,16");

    giet_assert( (ntasks <= NN ),
                 "[TRANSPOSE ERROR] number of tasks larger than number of lines");

    ///////////////////////////////////////////////////////////////////////
    // Processor [0,0,0] makes global initialisation
    // It includes parameters checking, heap and barrier initialization.
    // Others processors wait initialisation completion
    ///////////////////////////////////////////////////////////////////////

    if ( (x==0) && (y==0) && (lpid==0) )
    {
        // shared TTY allocation
        giet_tty_alloc( 1 );

        // TTY lock initialisation
        lock_init( &tty_lock);
      
        // distributed heap initialisation
        unsigned int cx , cy;
        for ( cx = 0 ; cx < x_size ; cx++ ) 
        {
            for ( cy = 0 ; cy < y_size ; cy++ ) 
            {
                heap_init( cx , cy );
            }
        }

        // barrier initialisation
        sqt_barrier_init( &barrier, x_size , y_size , nprocs );

        printf("\n[TRANSPOSE] Proc [0,0,0] completes initialisation at cycle %d\n",
               giet_proctime() );

        global_init_ok = 1;
    }
    else  
    {
        while ( global_init_ok == 0 );
    }
    
    ///////////////////////////////////////////////////////////////////////
    // In each cluster, only task running on processor[x,y,0] allocates 
    // the local buffers containing the images in the distributed heap
    // (one buf_in and one buf_out per cluster).
    // Other processors in cluster wait completion. 
    ///////////////////////////////////////////////////////////////////////

    if ( lpid == 0 ) 
    {
        buf_in[cluster_id]  = remote_malloc( npixels/nclusters, x, y );
        buf_out[cluster_id] = remote_malloc( npixels/nclusters, x, y );

        if ( (x==0) && (y==0) )
        printf("\n[TRANSPOSE] Proc [%d,%d,%d] completes buffer allocation"
               " for cluster[%d,%d] at cycle %d\n"
               " - buf_in  = %x\n"
               " - buf_out = %x\n",
               x, y, lpid, x, y, giet_proctime(), 
               (unsigned int)buf_in[cluster_id], (unsigned int)buf_out[cluster_id] );

        ///////////////////////////////////////////////////////////////////////
        // In each cluster, only task running on procesor[x,y,0] open the
        // three private file descriptors for the three files
        ///////////////////////////////////////////////////////////////////////

        // open initial file
        fd_initial = giet_fat_open( INITIAL_FILE_PATH , O_RDONLY );  // read_only
        if ( fd_initial < 0 ) 
        { 
            printf("\n[TRANSPOSE ERROR] Proc [%d,%d,%d] cannot open file %s\n",
                   x , y , lpid , INITIAL_FILE_PATH );
            giet_exit(" open() failure");
        }
        else if ( (x==0) && (y==0) && (lpid==0) )
        {
            printf("\n[TRANSPOSE] Proc [0,0,0] open file %s / fd = %d\n",
                   INITIAL_FILE_PATH , fd_initial );
        }

        // open transposed file
        fd_transposed = giet_fat_open( TRANSPOSED_FILE_PATH , O_CREATE );   // create if required
        if ( fd_transposed < 0 ) 
        { 
            printf("\n[TRANSPOSE ERROR] Proc [%d,%d,%d] cannot open file %s\n",
                            x , y , lpid , TRANSPOSED_FILE_PATH );
            giet_exit(" open() failure");
        }
        else if ( (x==0) && (y==0) && (lpid==0) )
        {
            printf("\n[TRANSPOSE] Proc [0,0,0] open file %s / fd = %d\n",
                   TRANSPOSED_FILE_PATH , fd_transposed );
        }

        // open restored file
        fd_restored = giet_fat_open( RESTORED_FILE_PATH , O_CREATE );   // create if required
        if ( fd_restored < 0 ) 
        { 
            printf("\n[TRANSPOSE ERROR] Proc [%d,%d,%d] cannot open file %s\n",
                   x , y , lpid , RESTORED_FILE_PATH );
            giet_exit(" open() failure");
        }
        else if ( (x==0) && (y==0) && (lpid==0) )
        {
            printf("\n[TRANSPOSE] Proc [0,0,0] open file %s / fd = %d\n",
                   RESTORED_FILE_PATH , fd_restored );
        }

        local_init_ok[x][y] = 1;
    }
    else
    {
        while( local_init_ok[x][y] == 0 );
    }

    ///////////////////////////////////////////////////////////////////////
    // Main loop / two iterations:
    // - first makes  initial    => transposed
    // - second makes transposed => restored 
    // All processors execute this main loop.
    ///////////////////////////////////////////////////////////////////////

    unsigned int fd_in  = fd_initial;
    unsigned int fd_out = fd_transposed;

    while (iteration < 2)
    {
        ///////////////////////////////////////////////////////////////////////
        // pseudo parallel load from disk to buf_in buffers: npixels/nclusters 
        // only task running on processor(x,y,0) does it
        ///////////////////////////////////////////////////////////////////////

        LOAD_START[x][y][lpid] = giet_proctime();

        if (lpid == 0)
        {
            unsigned int offset = ((npixels*cluster_id)/nclusters);
            if ( giet_fat_lseek( fd_in,
                                 offset,
                                 SEEK_SET ) != offset )
            {
                printf("\n[TRANSPOSE ERROR] Proc [%d,%d,%d] cannot seek fd = %d\n",
                       x , y , lpid , fd_in );
                giet_exit(" seek() failure");
            }

            unsigned int pixels = npixels / nclusters;
            if ( giet_fat_read( fd_in,
                                buf_in[cluster_id],
                                pixels ) != pixels )
            {
                printf("\n[TRANSPOSE ERROR] Proc [%d,%d,%d] cannot read fd = %d\n",
                       x , y , lpid , fd_in );
                giet_exit(" read() failure");
            }

            if ( (x==0) && (y==0) )
            printf("\n[TRANSPOSE] Proc [%d,%d,%d] completes load"
                   "  for iteration %d at cycle %d\n",
                   x, y, lpid, iteration, giet_proctime() );
        }

        LOAD_END[x][y][lpid] = giet_proctime();

        /////////////////////////////
        sqt_barrier_wait( &barrier );

        ///////////////////////////////////////////////////////////////////////
        // parallel transpose from buf_in to buf_out
        // each task makes the transposition for nlt lines (nlt = NN/ntasks)
        // from line [task_id*nlt] to line [(task_id + 1)*nlt - 1]
        // (p,l) are the absolute pixel coordinates in the source image
        ///////////////////////////////////////////////////////////////////////

        TRSP_START[x][y][lpid] = giet_proctime();

        unsigned int nlt   = NN / ntasks;      // number of lines per task
        unsigned int nlc   = NN / nclusters;   // number of lines per cluster

        unsigned int src_cluster;
        unsigned int src_index;
        unsigned int dst_cluster;
        unsigned int dst_index;

        unsigned char byte;

        unsigned int first = task_id * nlt;    // first line index for a given task
        unsigned int last  = first + nlt;      // last line index for a given task

        for ( l = first ; l < last ; l++ )
        {
            check_line_before[l] = 0;
         
            // in each iteration we transfer one byte
            for ( p = 0 ; p < NN ; p++ )
            {
                // read one byte from local buf_in
                src_cluster = l / nlc;
                src_index   = (l % nlc)*NN + p;
                byte        = buf_in[src_cluster][src_index];

                // compute checksum
                check_line_before[l] = check_line_before[l] + byte;

                // write one byte to remote buf_out
                dst_cluster = p / nlc; 
                dst_index   = (p % nlc)*NN + l;
                buf_out[dst_cluster][dst_index] = byte;
            }
        }

//        if ( lpid == 0 )
        {
//            if ( (x==0) && (y==0) )
            printf("\n[TRANSPOSE] proc [%d,%d,0] completes transpose"
                   " for iteration %d at cycle %d\n", 
                   x, y, iteration, giet_proctime() );

        }
        TRSP_END[x][y][lpid] = giet_proctime();

        /////////////////////////////
        sqt_barrier_wait( &barrier );

        ///////////////////////////////////////////////////////////////////////
        // parallel display from local buf_out to frame buffer
        // all tasks contribute to display using memcpy...
        ///////////////////////////////////////////////////////////////////////

        DISP_START[x][y][lpid] = giet_proctime();

        unsigned int  npt   = npixels / ntasks;   // number of pixels per task

        giet_fbf_sync_write( npt * task_id, 
                             &buf_out[cluster_id][lpid*npt], 
                             npt );

//        if ( (x==0) && (y==0) && (lpid==0) )
        printf("\n[TRANSPOSE] Proc [%d,%d,%d] completes display"
               " for iteration %d at cycle %d\n",
               x, y, lpid, iteration, giet_proctime() );

        DISP_END[x][y][lpid] = giet_proctime();

        /////////////////////////////
        sqt_barrier_wait( &barrier );

        ///////////////////////////////////////////////////////////////////////
        // pseudo parallel store : buf_out buffers to disk : npixels/nclusters 
        // only task running on processor(x,y,0) does it
        ///////////////////////////////////////////////////////////////////////

        STOR_START[x][y][lpid] = giet_proctime();

        if ( lpid == 0 )
        {
            unsigned int offset = ((npixels*cluster_id)/nclusters);
            if ( giet_fat_lseek( fd_out,
                                 offset,
                                 SEEK_SET ) != offset )
            {
                printf("\n[TRANSPOSE ERROR] Proc [%d,%d,%d] cannot seek fr = %d\n",
                       x , y , lpid , fd_out );
                giet_exit(" seek() failure");
            }

            unsigned int pixels = npixels / nclusters;
            if ( giet_fat_write( fd_out,
                                 buf_out[cluster_id],
                                 pixels ) != pixels )
            {
                printf("\n[TRANSPOSE ERROR] Proc [%d,%d,%d] cannot write fd = %d\n",
                       x , y , lpid , fd_out );
                giet_exit(" write() failure");
            }

            if ( (x==0) && (y==0) )
            printf("\n[TRANSPOSE] Proc [%d,%d,%d] completes store"
                   "  for iteration %d at cycle %d\n",
                   x, y, lpid, iteration, giet_proctime() );
        }

        STOR_END[x][y][lpid] = giet_proctime();

        /////////////////////////////
        sqt_barrier_wait( &barrier );

        // instrumentation done by processor [0,0,0] 
        if ( (x==0) && (y==0) && (lpid==0) && INSTRUMENTATION_OK )
        {
            int cx , cy , pp ;
            unsigned int min_load_start = 0xFFFFFFFF;
            unsigned int max_load_start = 0;
            unsigned int min_load_ended = 0xFFFFFFFF;
            unsigned int max_load_ended = 0;
            unsigned int min_trsp_start = 0xFFFFFFFF;
            unsigned int max_trsp_start = 0;
            unsigned int min_trsp_ended = 0xFFFFFFFF;
            unsigned int max_trsp_ended = 0;
            unsigned int min_disp_start = 0xFFFFFFFF;
            unsigned int max_disp_start = 0;
            unsigned int min_disp_ended = 0xFFFFFFFF;
            unsigned int max_disp_ended = 0;
            unsigned int min_stor_start = 0xFFFFFFFF;
            unsigned int max_stor_start = 0;
            unsigned int min_stor_ended = 0xFFFFFFFF;
            unsigned int max_stor_ended = 0;

            for (cx = 0; cx < x_size; cx++)
            {
            for (cy = 0; cy < y_size; cy++)
            {
            for (pp = 0; pp < NB_PROCS_MAX; pp++)
            {
                if (LOAD_START[cx][cy][pp] < min_load_start)  min_load_start = LOAD_START[cx][cy][pp];
                if (LOAD_START[cx][cy][pp] > max_load_start)  max_load_start = LOAD_START[cx][cy][pp];
                if (LOAD_END[cx][cy][pp]   < min_load_ended)  min_load_ended = LOAD_END[cx][cy][pp]; 
                if (LOAD_END[cx][cy][pp]   > max_load_ended)  max_load_ended = LOAD_END[cx][cy][pp];
                if (TRSP_START[cx][cy][pp] < min_trsp_start)  min_trsp_start = TRSP_START[cx][cy][pp];
                if (TRSP_START[cx][cy][pp] > max_trsp_start)  max_trsp_start = TRSP_START[cx][cy][pp];
                if (TRSP_END[cx][cy][pp]   < min_trsp_ended)  min_trsp_ended = TRSP_END[cx][cy][pp];
                if (TRSP_END[cx][cy][pp]   > max_trsp_ended)  max_trsp_ended = TRSP_END[cx][cy][pp];
                if (DISP_START[cx][cy][pp] < min_disp_start)  min_disp_start = DISP_START[cx][cy][pp];
                if (DISP_START[cx][cy][pp] > max_disp_start)  max_disp_start = DISP_START[cx][cy][pp];
                if (DISP_END[cx][cy][pp]   < min_disp_ended)  min_disp_ended = DISP_END[cx][cy][pp];
                if (DISP_END[cx][cy][pp]   > max_disp_ended)  max_disp_ended = DISP_END[cx][cy][pp];
                if (STOR_START[cx][cy][pp] < min_stor_start)  min_stor_start = STOR_START[cx][cy][pp];
                if (STOR_START[cx][cy][pp] > max_stor_start)  max_stor_start = STOR_START[cx][cy][pp];
                if (STOR_END[cx][cy][pp]   < min_stor_ended)  min_stor_ended = STOR_END[cx][cy][pp];
                if (STOR_END[cx][cy][pp]   > max_stor_ended)  max_stor_ended = STOR_END[cx][cy][pp];
            }
            }
            }

            printf("\n   ---------------- Instrumentation Results ---------------------\n");

            printf(" - LOAD_START : min = %d / max = %d / med = %d / delta = %d\n",
                   min_load_start, max_load_start, (min_load_start+max_load_start)/2, 
                   max_load_start-min_load_start); 

            printf(" - LOAD_END   : min = %d / max = %d / med = %d / delta = %d\n",
                   min_load_ended, max_load_ended, (min_load_ended+max_load_ended)/2, 
                   max_load_ended-min_load_ended); 

            printf(" - TRSP_START : min = %d / max = %d / med = %d / delta = %d\n",
                   min_trsp_start, max_trsp_start, (min_trsp_start+max_trsp_start)/2, 
                   max_trsp_start-min_trsp_start); 

            printf(" - TRSP_END   : min = %d / max = %d / med = %d / delta = %d\n",
                   min_trsp_ended, max_trsp_ended, (min_trsp_ended+max_trsp_ended)/2, 
                   max_trsp_ended-min_trsp_ended); 

            printf(" - DISP_START : min = %d / max = %d / med = %d / delta = %d\n",
                   min_disp_start, max_disp_start, (min_disp_start+max_disp_start)/2, 
                   max_disp_start-min_disp_start); 

            printf(" - DISP_END   : min = %d / max = %d / med = %d / delta = %d\n",
                   min_disp_ended, max_disp_ended, (min_disp_ended+max_disp_ended)/2, 
                   max_disp_ended-min_disp_ended); 

            printf(" - STOR_START : min = %d / max = %d / med = %d / delta = %d\n",
                   min_stor_start, max_stor_start, (min_stor_start+max_stor_start)/2, 
                   max_stor_start-min_stor_start); 

            printf(" - STOR_END   : min = %d / max = %d / med = %d / delta = %d\n",
                   min_stor_ended, max_stor_ended, (min_stor_ended+max_stor_ended)/2, 
                   max_stor_ended-min_stor_ended); 
        }

        /////////////////////////////
        sqt_barrier_wait( &barrier );

        // update iteration variables
        fd_in  = fd_transposed;
        fd_out = fd_restored;
        iteration++;

    } // end while      

    ///////////////////////////////////////////////////////////////////////
    // In each cluster, only task running on Processor[x,y,0] releases
    // the distributed buffers and close the file descriptors.
    ///////////////////////////////////////////////////////////////////////

    if ( lpid==0 )
    {
        free( buf_in[cluster_id] );
        free( buf_out[cluster_id] );

        giet_fat_close( fd_initial );
        giet_fat_close( fd_transposed );
        giet_fat_close( fd_restored );
    }

    giet_exit("Completed");

} // end main()

// Local Variables:
// tab-width: 3
// c-basic-offset: 
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3



