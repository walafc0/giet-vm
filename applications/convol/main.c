///////////////////////////////////////////////////////////////////////////////////////
// File   : main.c   (convol application)
// Date   : june 2014
// author : Alain Greiner
///////////////////////////////////////////////////////////////////////////////////////
// This multi-threaded application application implements a 2D convolution product.  
// The convolution kernel is [201]*[35] pixels, but it can be factored in two
// independant line and column convolution products.
// It can run on a multi-processors, multi-clusters architecture, with one thread
// per processor. 
// 
// The (1024 * 1024) pixels image is read from a file (2 bytes per pixel).
//
// - number of clusters containing processors must be power of 2 no larger than 256.
// - number of processors per cluster must be power of 2 no larger than 8.
///////////////////////////////////////////////////////////////////////////////////////

#include "stdio.h"
#include "stdlib.h"
#include "user_barrier.h"
#include "malloc.h"

#define USE_SQT_BARRIER            1
#define VERBOSE                    1
#define SUPER_VERBOSE              0

#define X_SIZE_MAX                 16
#define Y_SIZE_MAX                 16
#define PROCS_MAX                  8
#define CLUSTERS_MAX               (X_SIZE_MAX * Y_SIZE_MAX)

#define INITIAL_DISPLAY_ENABLE     0
#define FINAL_DISPLAY_ENABLE       1

#define PIXEL_SIZE                 2
#define NL                         1024
#define NP                         1024
#define NB_PIXELS                  (NP * NL)
#define FRAME_SIZE                 (NB_PIXELS * PIXEL_SIZE)

#define SEEK_SET                   0

#define TA(c,l,p)  (A[c][((NP) * (l)) + (p)])
#define TB(c,p,l)  (B[c][((NL) * (p)) + (l)])
#define TC(c,l,p)  (C[c][((NP) * (l)) + (p)])
#define TD(c,l,p)  (D[c][((NP) * (l)) + (p)])
#define TZ(c,l,p)  (Z[c][((NP) * (l)) + (p)])

#define max(x,y) ((x) > (y) ? (x) : (y))
#define min(x,y) ((x) < (y) ? (x) : (y))

// macro to use a shared TTY
#define printf(...)     lock_acquire( &tty_lock ); \
                        giet_tty_printf(__VA_ARGS__);  \
                        lock_release( &tty_lock )

// global instrumentation counters (cluster_id, lpid]

unsigned int START[CLUSTERS_MAX][PROCS_MAX];
unsigned int H_BEG[CLUSTERS_MAX][PROCS_MAX];
unsigned int H_END[CLUSTERS_MAX][PROCS_MAX];
unsigned int V_BEG[CLUSTERS_MAX][PROCS_MAX];
unsigned int V_END[CLUSTERS_MAX][PROCS_MAX];
unsigned int D_BEG[CLUSTERS_MAX][PROCS_MAX];
unsigned int D_END[CLUSTERS_MAX][PROCS_MAX];

// global synchronization barriers

#if USE_SQT_BARRIER
giet_sqt_barrier_t  barrier;
#else
giet_barrier_t      barrier;
#endif

volatile unsigned int barrier_init_ok    = 0;
volatile unsigned int load_image_ok      = 0;
volatile unsigned int instrumentation_ok = 0;

// lock protecting access to shared TTY
user_lock_t         tty_lock;

// global pointers on distributed buffers in all clusters
unsigned short * GA[CLUSTERS_MAX];
int *            GB[CLUSTERS_MAX];
int *            GC[CLUSTERS_MAX];
int *            GD[CLUSTERS_MAX];
unsigned char *  GZ[CLUSTERS_MAX];

///////////////////////////////////////////
__attribute__ ((constructor)) void main()
///////////////////////////////////////////
{
    //////////////////////////////////
    // convolution kernel parameters
    // The content of this section is
    // Philips proprietary information.
    ///////////////////////////////////

    int   vnorm  = 115;
    int   vf[35] = { 1, 1, 2, 2, 2,
                     2, 3, 3, 3, 4,
                     4, 4, 4, 5, 5,
                     5, 5, 5, 5, 5,
                     5, 5, 4, 4, 4,
                     4, 3, 3, 3, 2,
                     2, 2, 2, 1, 1 };

    int hrange = 100;
    int hnorm  = 201;

    unsigned int date = 0;

    int c; // cluster index for loops
    int l; // line index for loops
    int p; // pixel index for loops
    int z; // vertical filter index for loops

    // plat-form parameters
    unsigned int x_size;             // number of clusters in a row
    unsigned int y_size;             // number of clusters in a column
    unsigned int nprocs;             // number of processors per cluster
    
    giet_procs_number( &x_size , &y_size , &nprocs );

    // processor identifiers
    unsigned int x;                                         // x coordinate
    unsigned int y;                                         // y coordinate
    unsigned int lpid;                                      // local proc/task id
    giet_proc_xyp( &x, &y, &lpid );

    int          file       = 0;                            // file descriptor
    unsigned int nclusters  = x_size * y_size;              // number of clusters
    unsigned int cluster_id = (x * y_size) + y;             // continuous cluster index
    unsigned int task_id    = (cluster_id * nprocs) + lpid; // continuous task index
    unsigned int ntasks     = nclusters * nprocs;           // number of tasks
    unsigned int frame_size = FRAME_SIZE;                   // total size (bytes)

    unsigned int lines_per_task     = NL / ntasks;          // lines per task
    unsigned int lines_per_cluster  = NL / nclusters;       // lines per cluster
    unsigned int pixels_per_task    = NP / ntasks;          // columns per task
    unsigned int pixels_per_cluster = NP / nclusters;       // columns per cluster

    int first, last;

    date = giet_proctime();
    START[cluster_id][lpid] = date;

     // parameters checking 
   
    if ((nprocs != 1) && (nprocs != 2) && (nprocs != 4) && (nprocs != 8))
        giet_exit( "[CONVOL ERROR] NB_PROCS_MAX must be 1, 2, 4 or 8\n");

    if ((x_size!=1) && (x_size!=2) && (x_size!=4) && (x_size!=8) && (x_size!=16))
        giet_exit( "[CONVOL ERROR] x_size must be 1, 2, 4, 8, 16\n");
        
    if ((y_size!=1) && (y_size!=2) && (y_size!=4) && (y_size!=8) && (y_size!=16))
        giet_exit( "[CONVOL ERROR] y_size must be 1, 2, 4, 8, 16\n");

    if ( NL % nclusters != 0 )
        giet_exit( "[CONVOL ERROR] CLUSTERS_MAX must be a divider of NL");

    if ( NP % nclusters != 0 )
        giet_exit( "[CONVOL ERROR] CLUSTERS_MAX must be a divider of NP");

    
    ///////////////////////////////////////////////////////////////////
    // task[0][0][0] makes various initialisations
    ///////////////////////////////////////////////////////////////////
    
    if ( (x==0) && (y==0) && (lpid==0) )
    {
        // get a shared TTY
        giet_tty_alloc( 1 );

        // initializes TTY lock
        lock_init( &tty_lock );

        // initializes the distributed heap[x,y]
        unsigned int cx;
        unsigned int cy;
        for ( cx = 0 ; cx < x_size ; cx++ )
        {
            for ( cy = 0 ; cy < y_size ; cy++ )
            {
                heap_init( cx , cy );
            }
        }

#if USE_SQT_BARRIER
        sqt_barrier_init( &barrier, x_size , y_size , nprocs );
#else
        barrier_init( &barrier, ntasks );
#endif

        printf("\n[CONVOL] task[0,0,0] completes initialisation at cycle %d\n" 
               "- CLUSTERS   = %d\n"
               "- PROCS      = %d\n" 
               "- TASKS      = %d\n" 
               "- LINES/TASK = %d\n",
               giet_proctime(), nclusters, nprocs, ntasks, lines_per_task );

        barrier_init_ok = 1;
    }
    else
    {
        while ( barrier_init_ok == 0 );
    }

    ///////////////////////////////////////////////////////////////////
    // All task[x][y][0] allocate the global buffers in cluster(x,y)
    // These buffers mut be sector-aligned.
    ///////////////////////////////////////////////////////////////////
    if ( lpid == 0 )
    {

#if VERBOSE
printf( "\n[CONVOL] task[%d,%d,%d] enters malloc at cycle %d\n", 
                 x,y,lpid, date );
#endif

        GA[cluster_id] = remote_malloc( (FRAME_SIZE/nclusters)   , x , y );
        GB[cluster_id] = remote_malloc( (FRAME_SIZE/nclusters)*2 , x , y );
        GC[cluster_id] = remote_malloc( (FRAME_SIZE/nclusters)*2 , x , y );
        GD[cluster_id] = remote_malloc( (FRAME_SIZE/nclusters)*2 , x , y );
        GZ[cluster_id] = remote_malloc( (FRAME_SIZE/nclusters)/2 , x , y );
        
#if VERBOSE
printf( "\n[CONVOL]  Shared Buffer Virtual Addresses in cluster(%d,%d)\n"
        "### GA = %x\n"
        "### GB = %x\n"               
        "### GC = %x\n"               
        "### GD = %x\n"               
        "### GZ = %x\n",
        x, y,
        GA[cluster_id],
        GB[cluster_id],
        GC[cluster_id],
        GD[cluster_id],
        GZ[cluster_id] );
#endif
    }

    ///////////////////////////////
    #if USE_SQT_BARRIER
    sqt_barrier_wait( &barrier );
    #else
    barrier_wait( &barrier );
    #endif

    ///////////////////////////////////////////////////////////////////
    // All tasks initialise in their private stack a copy of the
    // arrays of pointers on the shared, distributed buffers.
    ///////////////////////////////////////////////////////////////////

    unsigned short * A[CLUSTERS_MAX];
    int            * B[CLUSTERS_MAX];
    int            * C[CLUSTERS_MAX];
    int            * D[CLUSTERS_MAX];
    unsigned char  * Z[CLUSTERS_MAX];

    for (c = 0; c < nclusters; c++)
    {
        A[c] = GA[c];
        B[c] = GB[c];
        C[c] = GC[c];
        D[c] = GD[c];
        Z[c] = GZ[c];
    }

    ///////////////////////////////////////////////////////////////////////////
    // task[0,0,0] open the file containing image, and load it from disk 
    // to all A[c] buffers (frame_size / nclusters loaded in each cluster).
    // Other tasks are waiting on the init_ok condition.
    //////////////////////////////////////////////////////////////////////////
    if ( (x==0) && (y==0) && (lpid==0) )
    {
        // open file
        file = giet_fat_open( "/misc/philips_1024.raw" , 0 );
        if ( file < 0 ) giet_exit( "[CONVOL ERROR] task[0,0,0] cannot open"
                                   " file /misc/philips_1024.raw" );
 
        printf( "\n[CONVOL] task[0,0,0] open file /misc/philips_1024.raw"
                " at cycle %d\n", giet_proctime() );

        for ( c = 0 ; c < nclusters ; c++ )
        {
            printf( "\n[CONVOL] task[0,0,0] starts load "
                    "for cluster %d at cycle %d\n", c, giet_proctime() );

            giet_fat_lseek( file,
                            (frame_size/nclusters)*c,
                            SEEK_SET );

            giet_fat_read( file,
                           A[c],
                           frame_size/nclusters );

            printf( "\n[CONVOL] task[0,0,0] completes load "
                    "for cluster %d at cycle %d\n", c, giet_proctime() );
        }
        load_image_ok = 1;
    }
    else
    {
        while ( load_image_ok == 0 );
    }

    /////////////////////////////////////////////////////////////////////////////
    // Optionnal parallel display of the initial image stored in A[c] buffers.
    // Eah task displays (NL/ntasks) lines. (one byte per pixel).
    /////////////////////////////////////////////////////////////////////////////

    if ( INITIAL_DISPLAY_ENABLE )
    {

#if VERBOSE
printf( "\n[CONVOL] task[%d,%d,%d] starts initial display"
        " at cycle %d\n",
        x, y, lpid, giet_proctime() );
#endif

        unsigned int line;
        unsigned int offset = lines_per_task * lpid;

        for ( l = 0 ; l < lines_per_task ; l++ )
        {
            line = offset + l;

            for ( p = 0 ; p < NP ; p++ )
            {
                TZ(cluster_id, line, p) = (unsigned char)(TA(cluster_id, line, p) >> 8);
            }

            giet_fbf_sync_write( NP*(l + (task_id * lines_per_task) ), 
                                 &TZ(cluster_id, line, 0), 
                                 NP);
        }

#if VERBOSE 
printf( "\n[CONVOL] task[%d,%d,%d] completes initial display"
        " at cycle %d\n",
        x, y, lpid, giet_proctime() );
#endif

        ////////////////////////////
        #if USE_SQT_BARRIER
        sqt_barrier_wait( &barrier );
        #else
        barrier_wait( &barrier );
        #endif

    }

    ////////////////////////////////////////////////////////
    // parallel horizontal filter : 
    // B <= transpose(FH(A))
    // D <= A - FH(A)
    // Each task computes (NL/ntasks) lines 
    // The image must be extended :
    // if (z<0)    TA(cluster_id,l,z) == TA(cluster_id,l,0)
    // if (z>NP-1) TA(cluster_id,l,z) == TA(cluster_id,l,NP-1)
    ////////////////////////////////////////////////////////

    date  = giet_proctime();
    H_BEG[cluster_id][lpid] = date;

#if VERBOSE 
printf( "\n[CONVOL] task[%d,%d,%d] starts horizontal filter"
        " at cycle %d\n",
        x, y, lpid, date );
#else
if ( (x==0) && (y==0) && (lpid==0) ) 
printf( "\n[CONVOL] task[0,0,0] starts horizontal filter"
        " at cycle %d\n", date );
#endif

    // l = absolute line index / p = absolute pixel index  
    // first & last define which lines are handled by a given task

    first = task_id * lines_per_task;
    last  = first + lines_per_task;

    for (l = first; l < last; l++)
    {
        // src_c and src_l are the cluster index and the line index for A & D
        int src_c = l / lines_per_cluster;
        int src_l = l % lines_per_cluster;

        // We use the specific values of the horizontal ep-filter for optimisation:
        // sum(p) = sum(p-1) + TA[p+hrange] - TA[p-hrange-1]
        // To minimize the number of tests, the loop on pixels is split in three domains 

        int sum_p = (hrange + 2) * TA(src_c, src_l, 0);
        for (z = 1; z < hrange; z++)
        {
            sum_p = sum_p + TA(src_c, src_l, z);
        }

        // first domain : from 0 to hrange
        for (p = 0; p < hrange + 1; p++)
        {
            // dst_c and dst_p are the cluster index and the pixel index for B
            int dst_c = p / pixels_per_cluster;
            int dst_p = p % pixels_per_cluster;
            sum_p = sum_p + (int) TA(src_c, src_l, p + hrange) - (int) TA(src_c, src_l, 0);
            TB(dst_c, dst_p, l) = sum_p / hnorm;
            TD(src_c, src_l, p) = (int) TA(src_c, src_l, p) - sum_p / hnorm;
        }
        // second domain : from (hrange+1) to (NP-hrange-1)
        for (p = hrange + 1; p < NP - hrange; p++)
        {
            // dst_c and dst_p are the cluster index and the pixel index for B
            int dst_c = p / pixels_per_cluster;
            int dst_p = p % pixels_per_cluster;
            sum_p = sum_p + (int) TA(src_c, src_l, p + hrange) 
                          - (int) TA(src_c, src_l, p - hrange - 1);
            TB(dst_c, dst_p, l) = sum_p / hnorm;
            TD(src_c, src_l, p) = (int) TA(src_c, src_l, p) - sum_p / hnorm;
        }
        // third domain : from (NP-hrange) to (NP-1)
        for (p = NP - hrange; p < NP; p++)
        {
            // dst_c and dst_p are the cluster index and the pixel index for B
            int dst_c = p / pixels_per_cluster;
            int dst_p = p % pixels_per_cluster;
            sum_p = sum_p + (int) TA(src_c, src_l, NP - 1) 
                          - (int) TA(src_c, src_l, p - hrange - 1);
            TB(dst_c, dst_p, l) = sum_p / hnorm;
            TD(src_c, src_l, p) = (int) TA(src_c, src_l, p) - sum_p / hnorm;
        }

#if SUPER_VERBOSE
printf(" - line %d computed at cycle %d\n", l, giet_proctime() );
#endif    

    }

    date  = giet_proctime();
    H_END[cluster_id][lpid] = date;

#if VERBOSE 
printf( "\n[CONVOL] task[%d,%d,%d] completes horizontal filter"
        " at cycle %d\n",
        x, y, lpid, date );
#else
if ( (x==0) && (y==0) && (lpid==0) ) 
printf( "\n[CONVOL] task[0,0,0] completes horizontal filter"
        " at cycle %d\n", date );
#endif

    /////////////////////////////
    #if USE_SQT_BARRIER
    sqt_barrier_wait( &barrier );
    #else
    barrier_wait( &barrier );
    #endif


    ///////////////////////////////////////////////////////////////
    // parallel vertical filter : 
    // C <= transpose(FV(B))
    // Each task computes (NP/ntasks) columns
    // The image must be extended :
    // if (l<0)    TB(cluster_id,p,l) == TB(cluster_id,p,0)
    // if (l>NL-1)   TB(cluster_id,p,l) == TB(cluster_id,p,NL-1)
    ///////////////////////////////////////////////////////////////

    date  = giet_proctime();
    V_BEG[cluster_id][lpid] = date;

#if VERBOSE 
printf( "\n[CONVOL] task[%d,%d,%d] starts vertical filter"
        " at cycle %d\n",
        x, y, lpid, date );
#else
if ( (x==0) && (y==0) && (lpid==0) ) 
printf( "\n[CONVOL] task[0,0,0] starts vertical filter"
        " at cycle %d\n", date );
#endif

    // l = absolute line index / p = absolute pixel index
    // first & last define which pixels are handled by a given task

    first = task_id * pixels_per_task;
    last  = first + pixels_per_task;

    for (p = first; p < last; p++)
    {
        // src_c and src_p are the cluster index and the pixel index for B
        int src_c = p / pixels_per_cluster;
        int src_p = p % pixels_per_cluster;

        int sum_l;

        // We use the specific values of the vertical ep-filter
        // To minimize the number of tests, the NL lines are split in three domains 

        // first domain : explicit computation for the first 18 values
        for (l = 0; l < 18; l++)
        {
            // dst_c and dst_l are the cluster index and the line index for C
            int dst_c = l / lines_per_cluster;
            int dst_l = l % lines_per_cluster;

            for (z = 0, sum_l = 0; z < 35; z++)
            {
                sum_l = sum_l + vf[z] * TB(src_c, src_p, max(l - 17 + z,0) );
            }
            TC(dst_c, dst_l, p) = sum_l / vnorm;
        }
        // second domain
        for (l = 18; l < NL - 17; l++)
        {
            // dst_c and dst_l are the cluster index and the line index for C
            int dst_c = l / lines_per_cluster;
            int dst_l = l % lines_per_cluster;

            sum_l = sum_l + TB(src_c, src_p, l + 4)
                  + TB(src_c, src_p, l + 8)
                  + TB(src_c, src_p, l + 11)
                  + TB(src_c, src_p, l + 15)
                  + TB(src_c, src_p, l + 17)
                  - TB(src_c, src_p, l - 5)
                  - TB(src_c, src_p, l - 9)
                  - TB(src_c, src_p, l - 12)
                  - TB(src_c, src_p, l - 16)
                  - TB(src_c, src_p, l - 18);

            TC(dst_c, dst_l, p) = sum_l / vnorm;
        }
        // third domain
        for (l = NL - 17; l < NL; l++)
        {
            // dst_c and dst_l are the cluster index and the line index for C
            int dst_c = l / lines_per_cluster;
            int dst_l = l % lines_per_cluster;

            sum_l = sum_l + TB(src_c, src_p, min(l + 4, NL - 1))
                  + TB(src_c, src_p, min(l + 8, NL - 1))
                  + TB(src_c, src_p, min(l + 11, NL - 1))
                  + TB(src_c, src_p, min(l + 15, NL - 1))
                  + TB(src_c, src_p, min(l + 17, NL - 1))
                  - TB(src_c, src_p, l - 5)
                  - TB(src_c, src_p, l - 9)
                  - TB(src_c, src_p, l - 12)
                  - TB(src_c, src_p, l - 16)
                  - TB(src_c, src_p, l - 18);

            TC(dst_c, dst_l, p) = sum_l / vnorm;
        }

#if SUPER_VERBOSE
printf(" - column %d computed at cycle %d\n", p, giet_proctime());
#endif 

    }

    date  = giet_proctime();
    V_END[cluster_id][lpid] = date;

#if VERBOSE 
printf( "\n[CONVOL] task[%d,%d,%d] completes vertical filter"
        " at cycle %d\n",
        x, y, lpid, date );
#else
if ( (x==0) && (y==0) && (lpid==0) ) 
printf( "\n[CONVOL] task[0,0,0] completes vertical filter"
        " at cycle %d\n", date );
#endif

    ////////////////////////////
    #if USE_SQT_BARRIER
    sqt_barrier_wait( &barrier );
    #else
    barrier_wait( &barrier );
    #endif

    ////////////////////////////////////////////////////////////////
    // Optional parallel display of the final image Z <= D + C
    // Eah task displays (NL/ntasks) lines. (one byte per pixel).
    ////////////////////////////////////////////////////////////////

    if ( FINAL_DISPLAY_ENABLE )
    {
        date  = giet_proctime();
        D_BEG[cluster_id][lpid] = date;

#if VERBOSE
printf( "\n[CONVOL] task[%d,%d,%d] starts final display"
        " at cycle %d\n",
        x, y, lpid, date);
#else
if ( (x==0) && (y==0) && (lpid==0) ) 
printf( "\n[CONVOL] task[0,0,0] starts final display"
        " at cycle %d\n", date );
#endif

        unsigned int line;
        unsigned int offset = lines_per_task * lpid;

        for ( l = 0 ; l < lines_per_task ; l++ )
        {
            line = offset + l;

            for ( p = 0 ; p < NP ; p++ )
            {
                TZ(cluster_id, line, p) = 
                   (unsigned char)( (TD(cluster_id, line, p) + 
                                     TC(cluster_id, line, p) ) >> 8 );
            }

            giet_fbf_sync_write( NP*(l + (task_id * lines_per_task) ), 
                                 &TZ(cluster_id, line, 0), 
                                 NP);
        }

        date  = giet_proctime();
        D_END[cluster_id][lpid] = date;

#if VERBOSE
printf( "\n[CONVOL] task[%d,%d,%d] completes final display"
        " at cycle %d\n",
        x, y, lpid, date);
#else
if ( (x==0) && (y==0) && (lpid==0) ) 
printf( "\n[CONVOL] task[0,0,0] completes final display"
        " at cycle %d\n", date );
#endif
     
    //////////////////////////////
    #if USE_SQT_BARRIER
    sqt_barrier_wait( &barrier );
    #else
    barrier_wait( &barrier );
    #endif

    }

    /////////////////////////////////////////////////////////
    // Task[0,0,0] makes the instrumentation 
    /////////////////////////////////////////////////////////

    if ( (x==0) && (y==0) && (lpid==0) )
    {
        date  = giet_proctime();
        printf("\n[CONVOL] task[0,0,0] starts instrumentation"
               " at cycle %d\n\n", date );

        int cc, pp;

        unsigned int min_start = 0xFFFFFFFF;
        unsigned int max_start = 0;

        unsigned int min_h_beg = 0xFFFFFFFF;
        unsigned int max_h_beg = 0;

        unsigned int min_h_end = 0xFFFFFFFF;
        unsigned int max_h_end = 0;

        unsigned int min_v_beg = 0xFFFFFFFF;
        unsigned int max_v_beg = 0;

        unsigned int min_v_end = 0xFFFFFFFF;
        unsigned int max_v_end = 0;

        unsigned int min_d_beg = 0xFFFFFFFF;
        unsigned int max_d_beg = 0;

        unsigned int min_d_end = 0xFFFFFFFF;
        unsigned int max_d_end = 0;

        for (cc = 0; cc < nclusters; cc++)
        {
            for (pp = 0; pp < nprocs; pp++ )
            {
                if (START[cc][pp] < min_start) min_start = START[cc][pp];
                if (START[cc][pp] > max_start) max_start = START[cc][pp];

                if (H_BEG[cc][pp] < min_h_beg) min_h_beg = H_BEG[cc][pp];
                if (H_BEG[cc][pp] > max_h_beg) max_h_beg = H_BEG[cc][pp];

                if (H_END[cc][pp] < min_h_end) min_h_end = H_END[cc][pp];
                if (H_END[cc][pp] > max_h_end) max_h_end = H_END[cc][pp];

                if (V_BEG[cc][pp] < min_v_beg) min_v_beg = V_BEG[cc][pp];
                if (V_BEG[cc][pp] > max_v_beg) max_v_beg = V_BEG[cc][pp];

                if (V_END[cc][pp] < min_v_end) min_v_end = V_END[cc][pp];
                if (V_END[cc][pp] > max_v_end) max_v_end = V_END[cc][pp];

                if (D_BEG[cc][pp] < min_d_beg) min_d_beg = D_BEG[cc][pp];
                if (D_BEG[cc][pp] > max_d_beg) max_d_beg = D_BEG[cc][pp];

                if (D_END[cc][pp] < min_d_end) min_d_end = D_END[cc][pp];
                if (D_END[cc][pp] > max_d_end) max_d_end = D_END[cc][pp];
            }
        }

        printf(" - START : min = %d / max = %d / med = %d / delta = %d\n",
               min_start, max_start, (min_start+max_start)/2, max_start-min_start);

        printf(" - H_BEG : min = %d / max = %d / med = %d / delta = %d\n",
               min_h_beg, max_h_beg, (min_h_beg+max_h_beg)/2, max_h_beg-min_h_beg);

        printf(" - H_END : min = %d / max = %d / med = %d / delta = %d\n",
               min_h_end, max_h_end, (min_h_end+max_h_end)/2, max_h_end-min_h_end);

        printf(" - V_BEG : min = %d / max = %d / med = %d / delta = %d\n",
               min_v_beg, max_v_beg, (min_v_beg+max_v_beg)/2, max_v_beg-min_v_beg);

        printf(" - V_END : min = %d / max = %d / med = %d / delta = %d\n",
               min_v_end, max_v_end, (min_v_end+max_v_end)/2, max_v_end-min_v_end);

        printf(" - D_BEG : min = %d / max = %d / med = %d / delta = %d\n",
               min_d_beg, max_d_beg, (min_d_beg+max_d_beg)/2, max_d_beg-min_d_beg);

        printf(" - D_END : min = %d / max = %d / med = %d / delta = %d\n",
               min_d_end, max_d_end, (min_d_end+max_d_end)/2, max_d_end-min_d_end);

        printf( "\n General Scenario (Kcycles for each step)\n" );
        printf( " - BOOT OS           = %d\n", (min_start            )/1000 );
        printf( " - LOAD IMAGE        = %d\n", (min_h_beg - min_start)/1000 );
        printf( " - H_FILTER          = %d\n", (max_h_end - min_h_beg)/1000 );
        printf( " - BARRIER HORI/VERT = %d\n", (min_v_beg - max_h_end)/1000 );
        printf( " - V_FILTER          = %d\n", (max_v_end - min_v_beg)/1000 );
        printf( " - BARRIER VERT/DISP = %d\n", (min_d_beg - max_v_end)/1000 );
        printf( " - DISPLAY           = %d\n", (max_d_end - min_d_beg)/1000 );

        instrumentation_ok = 1;
    }
    else
    {
        while ( instrumentation_ok == 0 );
    }

    giet_exit( "completed");

} // end main()

// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3


