///////////////////////////////////////////////////////////////////////////////////////
// File   : main.c   (for classif application)
// Date   : november 2014
// author : Alain Greiner
///////////////////////////////////////////////////////////////////////////////////////
// This multi-threaded application takes a stream of Gigabit Ethernet packets,
// and makes packet analysis and classification, based on the source MAC address.
// It uses the NIC peripheral, and the distributed kernel chbufs accessed by the CMA 
// component to receive and send packets on the Gigabit Ethernet port. 
//
// It can run on architectures containing up to 16 * 16 clusters,
// and up to 8 processors per cluster.
//
// It requires N+2 TTY terminals, as each task in cluster[0][0] displays messages.
//
// This application is described as a TCG (Task and Communication Graph) 
// containing (N+2) tasks per cluster:
// - one "load" task
// - one "store" task
// - N "analyse" tasks
// The containers are distributed (N+2 containers per cluster):
// - one RX container (part of the kernel rx_chbuf), in the kernel heap.
// - one TX container (part of the kernel tx-chbuf), in the kernel heap.
// - N working containers (one per analysis task), in the user heap.
// In each cluster, the "load", analysis" and "store" tasks communicates through
// three local MWMR fifos: 
// - fifo_l2a : tranfer a full container from "load" to "analyse" task.
// - fifo_a2s : transfer a full container from "analyse" to "store" task.
// - fifo_s2l : transfer an empty container from "store" to "load" task.
// For each fifo, one item is a 32 bits word defining the index of an
// available working container.
// The pointers on the working containers, and the pointers on the MWMR fifos
// are global arrays stored in cluster[0][0].
// a local MWMR fifo containing NB_PROCS_MAX containers (one item = one container).
// The MWMR fifo descriptors array is defined as a global variable in cluster[0][0]. 
//
// Initialisation is done in three steps by the "load" & "store" tasks:
// 1) Task "load" in cluster[0][0] initialises the heaps in all clusters. Other tasks
//    are waiting on the global_sync synchronisation variable.
// 2) Then task "load" in cluster[0][0] initialises the barrier between all "load"
//    tasks, allocates NIC & CMA RX channel, and starts the NIC_CMA RX transfer.
//    Other "load" tasks are waiting on the load_sync synchronisation variable.
//    Task "store" in cluster[0][0] initialises the barrier between all "store" tasks,
//    allocates NIC & CMA TX channels, and starts the NIC_CMA TX transfer.
//    Other "store" tasks are waiting on the store_sync synchronisation variable.
// 3) When this global initialisation is completed, the "load" task in all clusters
//    allocates the working containers and the MWMR fifos descriptors from the
//    user local heap. In each cluster, the "analyse" and "store" tasks are waiting
//    the local initialisation completion on the local_sync[x][y] variables.
//
// When initialisation is completed, all tasks loop on containers:
// 1) The "load" task get an empty working container from the fifo_s2l,
//    transfer one container from the kernel rx_chbuf to this user container,
//    and transfer ownership of this container to one "analysis" task by writing
//    into the fifo_l2a.    
// 2) The "analyse" task get one working container from the fifo_l2a, analyse
//    each packet header, compute the packet type (depending on the SRC MAC address),
//    increment the correspondint classification counter, and transpose the SRC
//    and the DST MAC addresses fot TX tranmission.
// 3) The "store" task transfer get a full working container from the fifo_a2s,
//    transfer this user container content to the the kernel tx_chbuf,
//    and transfer ownership of this empty container to the "load" task by writing
//    into the fifo_s2l.   
//     
// Instrumentation results display is done by the "store" task in cluster[0][0]
// when all "store" tasks completed the number of clusters specified by the
// CONTAINERS_MAX parameter.
///////////////////////////////////////////////////////////////////////////////////////

#include "stdio.h"
#include "user_barrier.h"
#include "malloc.h"
#include "user_lock.h"
#include "mwmr_channel.h"

#define X_SIZE_MAX      16
#define Y_SIZE_MAX      16
#define NPROCS_MAX      8
#define CONTAINERS_MAX  50
#define VERBOSE_ANALYSE 0

///////////////////////////////////////////////////////////////////////////////////////
//    Global variables
// The MWMR channels (descriptors and buffers), as well as the working containers 
// used by the "analysis" tasks are distributed in clusters.
// But the pointers on these distributed structures are stored in cluster[0][0].
///////////////////////////////////////////////////////////////////////////////////////

// pointers on distributed containers
unsigned int*       container[X_SIZE_MAX][Y_SIZE_MAX][NPROCS_MAX-2];  

// pointers on distributed mwmr fifos containing : temp[x][y][l] container descriptors
mwmr_channel_t*     mwmr_l2a[X_SIZE_MAX][Y_SIZE_MAX];  
mwmr_channel_t*     mwmr_a2s[X_SIZE_MAX][Y_SIZE_MAX];
mwmr_channel_t*     mwmr_s2l[X_SIZE_MAX][Y_SIZE_MAX]; 

// local synchros signaling local MWMR fifos initialisation completion
volatile unsigned int        local_sync[X_SIZE_MAX][Y_SIZE_MAX];  

// global synchro signaling global initialisation completion
volatile unsigned int        global_sync = 0;
volatile unsigned int        load_sync   = 0; 
volatile unsigned int        store_sync  = 0; 

// instrumentation counters
unsigned int        counter[16];

// distributed barrier between "load" tasks
giet_sqt_barrier_t  rx_barrier;

// distributed barrier between "store" tasks
giet_sqt_barrier_t  tx_barrier;

// NIC_RX and NIC_TX channel index
unsigned int        nic_rx_channel;
unsigned int        nic_tx_channel;

/////////////////////////////////////////
__attribute__ ((constructor)) void load()
/////////////////////////////////////////
{
    // each "load" task get platform parameters
    unsigned int    x_size;			// number of clusters in a row
    unsigned int    y_size;                     // number of clusters in a column
    unsigned int    nprocs;                     // number of processors per cluster
    giet_procs_number( &x_size, &y_size, &nprocs );

    giet_assert( (x_size <= X_SIZE_MAX) && 
                 (y_size <= Y_SIZE_MAX) && 
                 (nprocs <= NPROCS_MAX) , 
                 "[CLASSIF ERROR] illegal platform parameters" );

    // each "load" task get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    l;
    giet_proc_xyp( &x, &y, &l );

    // "load" task[0][0]
    // - initialises the heap in every cluster
    // - initialises barrier between all load tasks,
    // - allocates the NIC & CMA RX channels, and start the NIC_CMA RX transfer.
    // Other "load" tasks wait completion
    if ( (x==0) && (y==0) )
    {
        // allocate a private TTY
        giet_tty_alloc(0);

        giet_tty_printf("\n*** Task load on P[%d][%d][%d] starts at cycle %d\n"
                        "  x_size = %d / y_size = %d / nprocs = %d\n",
                        x , y , l , giet_proctime() , x_size, y_size, nprocs );

        unsigned int xid;  // x cluster coordinate index
        unsigned int yid;  // y cluster coordinate index

        for ( xid = 0 ; xid < x_size ; xid++ )
        {
	        for ( yid = 0 ; yid < y_size ; yid++ )
	        {
	            heap_init( xid, yid );
	        }
        }
    
	    global_sync = 1;

        sqt_barrier_init( &rx_barrier, x_size , y_size , 1 );
        nic_rx_channel = giet_nic_rx_alloc( x_size , y_size );
        giet_nic_rx_start( nic_rx_channel );
        load_sync = 1;
    }
    else
    {
        while ( load_sync == 0 ) asm volatile ("nop");
    }    

    // each load tasks allocates containers[x][y][n] (from local heap)
    // and register pointers in the local stack
    unsigned int   n;
    unsigned int*  cont[NPROCS_MAX-2]; 
    unsigned int   analysis_tasks = nprocs-2;

    for ( n = 0 ; n < analysis_tasks ; n++ )
    {
        container[x][y][n] = malloc( 4096 );
        cont[n]            = container[x][y][n];
    }
    
    // each load task allocates data buffers for mwmr fifos (from local heap)
    unsigned int*  data_l2a = malloc( analysis_tasks<<2 );
    unsigned int*  data_a2s = malloc( analysis_tasks<<2 );
    unsigned int*  data_s2l = malloc( analysis_tasks<<2 );

    // each load task allocates mwmr fifos descriptors (from local heap)
    mwmr_l2a[x][y] = malloc( sizeof(mwmr_channel_t) );
    mwmr_a2s[x][y] = malloc( sizeof(mwmr_channel_t) );
    mwmr_s2l[x][y] = malloc( sizeof(mwmr_channel_t) );

    // each load task registers local pointers on mwmr fifos in local stack
    mwmr_channel_t* fifo_l2a = mwmr_l2a[x][y];
    mwmr_channel_t* fifo_a2s = mwmr_a2s[x][y];
    mwmr_channel_t* fifo_s2l = mwmr_s2l[x][y];

    // each load task initialises local mwmr fifos descriptors
    // ( width = 4 bytes / depth = number of analysis tasks )
    mwmr_init( fifo_l2a , data_l2a , 1 , analysis_tasks );
    mwmr_init( fifo_a2s , data_a2s , 1 , analysis_tasks );
    mwmr_init( fifo_s2l , data_s2l , 1 , analysis_tasks );

    
    // each load task initialises local containers as empty in fifo_s2l
    for ( n = 0 ; n < analysis_tasks ; n++ ) mwmr_write( fifo_s2l , &n , 1 );

    // each load task[x][y] signals mwmr fifos initialisation completion
    // to other tasks in same cluster[x][y]
    local_sync[x][y] = 1;

    // load task[0][0] displays status
    if ( (x==0) && (y==0) )
    giet_tty_printf("\n*** Task load on P[%d,%d,%d] enters main loop at cycle %d\n"
                    "      nic_rx_channel = %d / nic_tx_channel = %d\n"
                    "      &mwmr_l2a  = %x / &data_l2a  = %x\n"
                    "      &mwmr_a2s  = %x / &data_a2s  = %x\n"
                    "      &mwmr_s2l  = %x / &data_s2l  = %x\n"
                    "      &cont[0]   = %x\n"
                    "      x_size = %d / y_size = %d / nprocs = %d\n",
                    x , y , l , giet_proctime(), 
                    nic_rx_channel , nic_tx_channel,
                    (unsigned int)fifo_l2a, (unsigned int)data_l2a,
                    (unsigned int)fifo_a2s, (unsigned int)data_a2s,
                    (unsigned int)fifo_s2l, (unsigned int)data_s2l,
                    (unsigned int)cont[0],
                    x_size, y_size, nprocs );
 
    /////////////////////////////////////////////////////////////
    // All load tasks enter the main loop (on containers)
    unsigned int  count = 0;     // loaded containers count
    unsigned int  index;         // available container index
    unsigned int* temp;          // pointer on available container

    while ( count < CONTAINERS_MAX ) 
    { 
        // get one empty container index from fifo_s2l
        mwmr_read( fifo_s2l , &index , 1 );
        temp = cont[index];

        // get one container from  kernel rx_chbuf
        giet_nic_rx_move( nic_rx_channel, temp );

        // get packets number
        unsigned int npackets = temp[0] & 0x0000FFFF;
        unsigned int nwords   = temp[0] >> 16;

        if ( (x==0) && (y==0) )
        giet_tty_printf("\n*** Task load on P[%d,%d,%d] get container %d at cycle %d"
                        " : %d packets / %d words\n",
                        x, y, l, count, giet_proctime(), npackets, nwords );

        // put the full container index to fifo_l2a
        mwmr_write( fifo_l2a, &index , 1 );

        count++;
    }

    // all "load" tasks synchronise before stats
    sqt_barrier_wait( &rx_barrier );

    // "load" task[0][0] stops the NIC_CMA RX transfer and displays stats
    if ( (x==0) && (y==0) ) 
    {
        giet_nic_rx_stop( nic_rx_channel );
        giet_nic_rx_stats( nic_rx_channel );
    }

    // all "load" task exit
    giet_exit("Task completed");
 
} // end load()


//////////////////////////////////////////
__attribute__ ((constructor)) void store()
//////////////////////////////////////////
{
    // each "load" task get platform parameters
    unsigned int    x_size;						// number of clusters in row
    unsigned int    y_size;                     // number of clusters in a column
    unsigned int    nprocs;                     // number of processors per cluster
    giet_procs_number( &x_size, &y_size, &nprocs );

    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    l;
    giet_proc_xyp( &x, &y, &l );

    // "Store" tasks wait completion of heaps initialization
    while ( global_sync == 0 ) asm volatile ("nop");

    // "store" task[0][0] initialises the barrier between all "store" tasks,
    // allocates NIC & CMA TX channels, and starts the NIC_CMA TX transfer.
    // Other "store" tasks wait completion.
    if ( (x==0) && (y==0) )
    {
        // allocate a private TTY
        giet_tty_alloc(0);

        giet_tty_printf("\n*** Task store on P[%d][%d][%d] starts at cycle %d\n"
                        "  x_size = %d / y_size = %d / nprocs = %d\n",
                        x , y , l , giet_proctime() , x_size, y_size, nprocs );
 
        sqt_barrier_init( &tx_barrier , x_size , y_size , 1 );
        nic_tx_channel = giet_nic_tx_alloc( x_size , y_size );
        giet_nic_tx_start( nic_tx_channel );
        store_sync = 1;
    }
    else
    {
        while ( store_sync == 0 ) asm volatile ("nop");
    }    

    // all "store" tasks wait mwmr channels initialisation
    while ( local_sync[x][y] == 0 ) asm volatile ("nop");

    // each "store" tasks register pointers on working containers in local stack
    unsigned int   n;
    unsigned int   analysis_tasks = nprocs-2;
    unsigned int*  cont[NPROCS_MAX-2]; 

    for ( n = 0 ; n < analysis_tasks ; n++ )
    {
        cont[n] = container[x][y][n];
    }
    
    // all "store" tasks register pointers on mwmr fifos in local stack
    mwmr_channel_t* fifo_l2a = mwmr_l2a[x][y];
    mwmr_channel_t* fifo_a2s = mwmr_a2s[x][y];
    mwmr_channel_t* fifo_s2l = mwmr_s2l[x][y];

    // "store" task[0][0] displays status
    if ( (x==0) && (y==0) )
    giet_tty_printf("\n*** Task store on P[%d,%d,%d] enters main loop at cycle %d\n"
                    "      &mwmr_l2a  = %x\n"
                    "      &mwmr_a2s  = %x\n"
                    "      &mwmr_s2l  = %x\n"
                    "      &cont[0]   = %x\n",
                    x , y , l , giet_proctime(), 
                    (unsigned int)fifo_l2a,
                    (unsigned int)fifo_a2s,
                    (unsigned int)fifo_s2l,
                    (unsigned int)cont[0] );


    /////////////////////////////////////////////////////////////
    // all "store" tasks enter the main loop (on containers)
    unsigned int count = 0;     // stored containers count
    unsigned int index;         // empty container index
    unsigned int* temp;         // pointer on empty container

    while ( count < CONTAINERS_MAX ) 
    { 
        // get one working container index from fifo_a2s
        mwmr_read( fifo_a2s , &index , 1 );
        temp = cont[index];

        // put one container to  kernel tx_chbuf
        giet_nic_tx_move( nic_tx_channel, temp );

        // get packets number
        unsigned int npackets = temp[0] & 0x0000FFFF;
        unsigned int nwords   = temp[0] >> 16;

        if ( (x==0) && (y==0) )
        giet_tty_printf("\n*** Task store on P[%d,%d,%d] get container %d at cycle %d"
                        " : %d packets / %d words\n",
                        x, y, l, count, giet_proctime(), npackets, nwords );

        // put the working container index to fifo_s2l
        mwmr_write( fifo_s2l, &index , 1 );

        count++;
    }

    // all "store" tasks synchronise before result display
    sqt_barrier_wait( &tx_barrier );

    // "store" task[0,0] stops NIC_CMA TX transfer and displays results 
    if ( (x==0) && (y==0) )
    {
        giet_nic_tx_stop( nic_tx_channel );

        giet_tty_printf("\n@@@@ Classification Results @@@\n"
                        " - TYPE 0 : %d packets\n"
                        " - TYPE 1 : %d packets\n"
                        " - TYPE 2 : %d packets\n"
                        " - TYPE 3 : %d packets\n"
                        " - TYPE 4 : %d packets\n"
                        " - TYPE 5 : %d packets\n"
                        " - TYPE 6 : %d packets\n"
                        " - TYPE 7 : %d packets\n"
                        " - TYPE 8 : %d packets\n"
                        " - TYPE 9 : %d packets\n"
                        " - TYPE A : %d packets\n"
                        " - TYPE B : %d packets\n"
                        " - TYPE C : %d packets\n"
                        " - TYPE D : %d packets\n"
                        " - TYPE E : %d packets\n"
                        " - TYPE F : %d packets\n"
                        "    TOTAL = %d packets\n",
                        counter[0x0], counter[0x1], counter[0x2], counter[0x3],
                        counter[0x4], counter[0x5], counter[0x6], counter[0x7],
                        counter[0x8], counter[0x9], counter[0xA], counter[0xB],
                        counter[0xC], counter[0xD], counter[0xE], counter[0xF],
                        counter[0x0]+ counter[0x1]+ counter[0x2]+ counter[0x3]+
                        counter[0x4]+ counter[0x5]+ counter[0x6]+ counter[0x7]+
                        counter[0x8]+ counter[0x9]+ counter[0xA]+ counter[0xB]+
                        counter[0xC]+ counter[0xD]+ counter[0xE]+ counter[0xF] );

        giet_nic_tx_stats( nic_tx_channel );
    }

    // all "store" task exit
    giet_exit("Task completed");

} // end store()


////////////////////////////////////////////
__attribute__ ((constructor)) void analyse()
////////////////////////////////////////////
{
    // each "load" task get platform parameters
    unsigned int    x_size;						// number of clusters in row
    unsigned int    y_size;                     // number of clusters in a column
    unsigned int    nprocs;                     // number of processors per cluster
    giet_procs_number( &x_size, &y_size, &nprocs );

    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    l;
    giet_proc_xyp( &x, &y, &l );

    if ( (x==0) && (y==0) )
    {
        // allocate a private TTY
        giet_tty_alloc(0);

        giet_tty_printf("\n*** Task analyse on P[%d][%d][%d] starts at cycle %d\n"
                        "  x_size = %d / y_size = %d / nprocs = %d\n",
                        x , y , l , giet_proctime() , x_size, y_size, nprocs );
    }

    // all "analyse" tasks wait heaps and mwmr channels initialisation
    while ( local_sync[x][y] == 0 ) asm volatile ("nop");

    // all "analyse" tasks register pointers on working containers in local stack
    unsigned int   n;
    unsigned int   analysis_tasks = nprocs-2;
    unsigned int*  cont[NPROCS_MAX-2]; 
    for ( n = 0 ; n < analysis_tasks ; n++ )
    {
        cont[n] = container[x][y][n];
    }

    // all "analyse" tasks register pointers on mwmr fifos in local stack
    mwmr_channel_t* fifo_l2a = mwmr_l2a[x][y];
    mwmr_channel_t* fifo_a2s = mwmr_a2s[x][y];

    // "analyse" task[0][0] display status
    if ( (x==0) && (y==0) )
    giet_tty_printf("\n*** Task analyse on P[%d,%d,%d] enters main loop at cycle %d\n"
                    "       &mwmr_l2a = %x\n"
                    "       &mwmr_a2s = %x\n"
                    "       &cont[0]  = %x\n",
                    x, y, l, giet_proctime(), 
                    (unsigned int)fifo_l2a,
                    (unsigned int)fifo_a2s,
                    (unsigned int)cont[0] );
      
    /////////////////////////////////////////////////////////////
    // all "analyse" tasks enter the main loop (on containers)
    unsigned int  index;           // available container index
    unsigned int* temp;            // pointer on available container
    unsigned int  nwords;          // number of words in container
    unsigned int  npackets;        // number of packets in container
    unsigned int  length;          // number of bytes in current packet
    unsigned int  first;           // current packet first word in container
    unsigned int  type;            // current packet type
    unsigned int  p;               // current packet index

#if VERBOSE_ANALYSE
    unsigned int       verbose_len[10]; // save length for all packets in one container
    unsigned long long verbose_dst[10]; // save length for all packets in one container
    unsigned long long verbose_src[10]; // save length for all packets in one container
#endif

    while ( 1 )
    { 

#if VERBOSE_ANALYSE 
            for( p = 0 ; p < 10 ; p++ )
            {
                verbose_len[p] = 0;
                verbose_dst[p] = 0;
                verbose_src[p] = 0;
            }
#endif
        // get one working container index from fifo_l2a
        mwmr_read( fifo_l2a , &index , 1 );
        temp = cont[index];

        // get packets number and words number
        npackets = temp[0] & 0x0000FFFF;
        nwords   = temp[0] >> 16;

        if ( (x==0) && (y==0) )
        giet_tty_printf("\n*** Task analyse on P[%d,%d,%d] get container at cycle %d"
                        " : %d packets / %d words\n",
						x, y, l, giet_proctime(), npackets, nwords );

        // initialize word index in container
        first = 34;

        // loop on packets
        for( p = 0 ; p < npackets ; p++ )
        {
            // get packet length from container header
            if ( (p & 0x1) == 0 )  length = temp[1+(p>>1)] >> 16;
            else                   length = temp[1+(p>>1)] & 0x0000FFFF;

            // compute packet DST and SRC MAC addresses
            unsigned int word0 = temp[first];
            unsigned int word1 = temp[first + 1];
            unsigned int word2 = temp[first + 2];

#if VERBOSE_ANALYSE 
            unsigned long long dst = ((unsigned long long)(word1 & 0xFFFF0000)>>16) |
                                     (((unsigned long long)word0)<<16);
            unsigned long long src = ((unsigned long long)(word1 & 0x0000FFFF)<<32) |
                                     ((unsigned long long)word2);
            if ( p < 10 )
            {
                verbose_len[p] = length;
                verbose_dst[p] = dst;
                verbose_src[p] = src;
            }
#endif
            // compute type from SRC MAC address and increment counter
            type = word1 & 0x0000000F;
            atomic_increment( &counter[type], 1 );

            // exchange SRC & DST MAC addresses for TX
            temp[first]     = ((word1 & 0x0000FFFF)<<16) | ((word2 & 0xFFFF0000)>>16);
            temp[first + 1] = ((word2 & 0x0000FFFF)<<16) | ((word0 & 0xFFFF0000)>>16);
            temp[first + 2] = ((word0 & 0x0000FFFF)<<16) | ((word1 & 0xFFFF0000)>>16);

            // update first word index 
            if ( length & 0x3 ) first += (length>>2)+1;
            else                first += (length>>2);
        }
        
#if VERBOSE_ANALYSE 
        if ( (x==0) && (y==0) )
        giet_tty_printf("\n*** Task analyse on P[%d,%d,%d] completes at cycle %d\n"
                        "   - Packet 0 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 1 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 2 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 3 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 4 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 5 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 6 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 7 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 8 : plen = %d / dst_mac = %l / src_mac = %l\n"
                        "   - Packet 9 : plen = %d / dst_mac = %l / src_mac = %l\n",
                        x , y , l , giet_proctime() , 
                        verbose_len[0] , verbose_dst[0] , verbose_src[0] ,
                        verbose_len[1] , verbose_dst[1] , verbose_src[1] ,
                        verbose_len[2] , verbose_dst[2] , verbose_src[2] ,
                        verbose_len[3] , verbose_dst[3] , verbose_src[3] ,
                        verbose_len[4] , verbose_dst[4] , verbose_src[4] ,
                        verbose_len[5] , verbose_dst[5] , verbose_src[5] ,
                        verbose_len[6] , verbose_dst[6] , verbose_src[6] ,
                        verbose_len[7] , verbose_dst[7] , verbose_src[7] ,
                        verbose_len[8] , verbose_dst[8] , verbose_src[8] ,
                        verbose_len[9] , verbose_dst[9] , verbose_src[9] );
#endif
            
        // pseudo-random delay
        unsigned int delay = giet_rand()>>3;
        for( p = 0 ; p < delay ; p++ ) asm volatile ("nop");

        // put the working container index to fifo_a2s
        mwmr_write( fifo_a2s , &index , 1 );
    }
} // end analyse()

