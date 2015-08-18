/////////////////////////////////////////////////////////////////////////////////////////////
// File   : main.c   (for router application)
// Date   : november 2014
// author : Alain Greiner
/////////////////////////////////////////////////////////////////////////////////////////////
// This multi-threaded application illustrates "task-farm" parallelism.
// It is described as a TCG (Task and Communication Graph).
// It contains 2 + N tasks : one "producer", one "consumer" and N "router")
// It contains 2 MWMR channels : "fifo_in" and "fifo_out".
// - The "producer" task writes NMAX token into "fifo_in".
// - The N "router" tasks read token from "fifo_in" and write them into "fifo_out".
// - The "consumer" task read token from "fifo_out" and displays instrumentation results.
// Token are indexed (by the producer) from 0 to NMAX-1.
// The router task contains a random delay emulating a variable processing time.
//
// This application is intended to run on a multi-processors, multi-clusters architecture,
//  with one thread per processor. 
//
// It uses the he following hardware parameters, defined in the hard_config.h file:
// - X_SIZE       : number of clusters in a row
// - Y_SIZE       : number of clusters in a column
// - NB_PROCS_MAX : number of processors per cluster
// 
// There is two global arrays (indexed by the token index) for insrumentation:
// - The "router_tab" array is filled concurrently by all "router" tasks.
//   Each entry contains the processor index that routed the token.
// - The "consumer_tab" array is filled by the "consumer" task.
//   Each entry contain the arrival order to the consumer task.
/////////////////////////////////////////////////////////////////////////////////////////////
// Implementation note:
// The synchronisation variables fifo_in, fifo_out, and tty_lock are initialised by the
// "producer" task. Other tasks are waiting on the init_ok variable until completion.
/////////////////////////////////////////////////////////////////////////////////////////////


#include "stdio.h"
#include "mwmr_channel.h"
#include "mapping_info.h"
#include "hard_config.h"

#define VERBOSE  1
#define NMAX     50	      // total number of token
#define DEPTH    20       // MWMR channels depth

// MWMR channels and associated buffers

__attribute__((section (".data_in")))  mwmr_channel_t fifo_in;
__attribute__((section (".data_in")))  unsigned int   buf_in[DEPTH];

__attribute__((section (".data_out"))) mwmr_channel_t fifo_out;
__attribute__((section (".data_out"))) unsigned int   buf_out[DEPTH];
 
// Instrumentation Counters

__attribute__((section (".data_out")))  unsigned int consumer_tab[NMAX];
__attribute__((section (".data_out")))  unsigned int router_tab[NMAX];

// synchronisation variables

unsigned int  init_ok = 0;

/////////////////////////////////////////////
__attribute__ ((constructor)) void producer()
{

    unsigned int 	n;
    unsigned int 	buf;

    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    p;
    giet_proc_xyp( &x , &y , &p );

    // allocates a private TTY
    giet_tty_alloc( 0 );

    // initialises TTY lock
    // lock_init( &tty_lock );

    // initializes fifo_in
    mwmr_init( &fifo_in  , buf_in  , 1 , DEPTH );

    // initializes fifo_out
    mwmr_init( &fifo_out , buf_out , 1 , DEPTH );

    init_ok = 1;

    giet_tty_printf("\n[Producer] completes initialisation on P[%d,%d,%d] at cycle %d\n", 
                    x , y , p , giet_proctime() );

    // main loop 
    for(n = 0 ; n < NMAX ; n++) 
    { 
        buf = n;
        mwmr_write( &fifo_in , &buf , 1 );

        if ( VERBOSE ) 
        giet_tty_printf(" - token %d sent at cycle %d\n", n , giet_proctime() );
    }

    giet_exit( "Producer task completed");

} // end producer()

/////////////////////////////////////////////
__attribute__ ((constructor)) void consumer()
{
    unsigned int 	n;
    unsigned int 	buf;

    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    p;
    giet_proc_xyp( &x, &y, &p );

    // allocates a private TTY
    giet_tty_alloc( 0 );

    while ( init_ok == 0 ) asm volatile( "nop" );

    giet_tty_printf("\n[Consumer] starts execution on P[%d,%d,%d] at cycle %d\n", 
                    x, y, p, giet_proctime() );

    // main loop 
    for( n = 0 ; n < NMAX ; n++ ) 
    { 
        mwmr_read( &fifo_out , &buf , 1 );
        consumer_tab[n] = buf;

        if ( VERBOSE ) 
        giet_tty_printf(" - token %d received at cycle %d\n", buf , giet_proctime() );
    }

    // instrumentation display
    giet_tty_printf("\n[Consumer] displays instrumentation results\n");
    for( n = 0 ; n < NMAX ; n++ )
    {
        giet_tty_printf(" - arrival = %d / value = %d / router = %x\n",
                        n , consumer_tab[n] , router_tab[n] );
    }

    giet_exit( "Consumer completed" );

} // end consumer()

///////////////////////////////////////////
__attribute__ ((constructor)) void router()
{
    unsigned int 	buf;
    unsigned int 	n;
    unsigned int	tempo;

    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    p;
    giet_proc_xyp( &x, &y, &p );

    // allocates a private TTY
    giet_tty_alloc( 0 );

    giet_tty_printf("\n[Router] starts execution on P[%d,%d,%d] at cycle %d\n", 
                    x, y, p, giet_proctime() );

    while ( init_ok == 0 ) asm volatile( "nop" );

    // main loop
    while(1)
    {
        mwmr_read( &fifo_in , &buf , 1 );

        tempo = giet_rand();
        for ( n = 0 ; n < tempo ; n++ ) asm volatile ( "nop" );

        router_tab[buf] = (x<<(Y_WIDTH + P_WIDTH)) + (y<<P_WIDTH) + p;

        mwmr_write( &fifo_out , &buf , 1 );

        if ( VERBOSE ) 
        giet_tty_printf(" - token %d routed at cycle %d\n", buf , giet_proctime() );
    }
} // end router
