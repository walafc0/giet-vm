///////////////////////////////////////////////////////////////////////////////////////
//  file   : main.c  (for coproc application)
//  date   : avril 2015
//  author : Alain Greiner
///////////////////////////////////////////////////////////////////////////////////////
//  This file describes the single thread "coproc" application.
//  It uses the embedded GCD (Greater Common Divider) coprocessor to make
//  the GCD computation between two vectors of 32 bits integers.
//  The vectors size is defined by the VECTOR_SIZE parameter.
///////////////////////////////////////////////////////////////////////////////////////


#include "stdio.h"
#include "mapping_info.h"       // for coprocessors types an modes

#define  VECTOR_SIZE 128   

#define  DMA_MODE    MODE_DMA_IRQ

#define VERBOSE      1

// Memory buffers for coprocessor
unsigned int opa[VECTOR_SIZE] __attribute__((aligned(64)));
unsigned int opb[VECTOR_SIZE] __attribute__((aligned(64)));
unsigned int res[VECTOR_SIZE] __attribute__((aligned(64)));

/////////////////////////////////////////
__attribute__ ((constructor)) void main()
{
    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    lpid;
    giet_proc_xyp( &x, &y, &lpid );

    // get a private TTY terminal
    giet_tty_alloc( 0 );

    giet_tty_printf("\n*** Starting coproc application on processor"
                    "[%d,%d,%d] at cycle %d\n", 
                    x, y, lpid, giet_proctime() );

    // initializes opa & opb buffers
    unsigned int word;
    for ( word = 0 ; word < VECTOR_SIZE ; word++ )
    {
        opa[word] = giet_rand() + 1;
        opb[word] = giet_rand() + 1;
    }

    unsigned int coproc_info;

    /////////////////////// request a GCD coprocessor
    giet_coproc_alloc( MWR_SUBTYPE_GCD, &coproc_info );

    // check coprocessor ports
    unsigned int nb_to_coproc   = (coproc_info    ) & 0xFF;
    unsigned int nb_from_coproc = (coproc_info>> 8) & 0xFF;
    unsigned int nb_config      = (coproc_info>>16) & 0xFF;
    unsigned int nb_status      = (coproc_info>>24) & 0xFF;
    giet_assert( ((nb_to_coproc   == 2) &&
                  (nb_from_coproc == 1) &&
                  (nb_config      == 1) &&
                  (nb_status      == 0) ) ,
                  "wrong GCD coprocessor interface" );

if ( VERBOSE )
giet_tty_printf("\n*** get GCD coprocessor at cycle %d\n", giet_proctime() );

    //////////////////////// initializes channel for OPA
    giet_coproc_channel_t opa_desc;
    opa_desc.channel_mode = DMA_MODE;
    opa_desc.buffer_size  = VECTOR_SIZE<<2;
    opa_desc.buffer_vaddr = (unsigned int)opa;
    giet_coproc_channel_init( 0 , &opa_desc );
    
    //////////////////////// initializes channel for OPB
    giet_coproc_channel_t opb_desc;
    opb_desc.channel_mode = DMA_MODE;
    opb_desc.buffer_size  = VECTOR_SIZE<<2;
    opb_desc.buffer_vaddr = (unsigned int)opb;
    giet_coproc_channel_init( 1 , &opb_desc );
    
    //////////////////////// initializes channel for RES
    giet_coproc_channel_t res_desc;
    res_desc.channel_mode = DMA_MODE;
    res_desc.buffer_size  = VECTOR_SIZE<<2;
    res_desc.buffer_vaddr = (unsigned int)res;
    giet_coproc_channel_init( 2 , &res_desc );
    
if ( VERBOSE )
giet_tty_printf("\n*** channels initialized at cycle %d\n", giet_proctime() );

    /////////////////////// starts communication channels
    giet_coproc_run( 0 );

if ( VERBOSE )
giet_tty_printf("\n*** start GCD coprocessor at cycle %d\n", giet_proctime() );

    /////////////////////// wait coprocessor completion
    if ( DMA_MODE == MODE_DMA_NO_IRQ )
    {
        giet_coproc_completed( );
    }

if ( VERBOSE )
giet_tty_printf("\n*** GCD computation completed at cycle %d\n", giet_proctime() );

    // display result
    for ( word = 0 ; word < VECTOR_SIZE ; word++ )
    {
        giet_tty_printf("pgcd( %d , %d ) = %d\n",
        opa[word] , opb[word] , res[word] );
    }

    ////////////////////// release GCD coprocessor
    giet_coproc_release( 0 );

    giet_exit("completed");

} // end main

