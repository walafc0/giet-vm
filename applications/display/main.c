///////////////////////////////////////////////////////////////////////////////////////
//  file   : main.c     (display application)
//  date   : may 2014
//  author : Alain Greiner
///////////////////////////////////////////////////////////////////////////////////////
//  This file describes the single thread "display" application.
//  It uses the external chained buffer DMA to display a stream
//  of images on the frame buffer.  
///////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <hard_config.h>     // To check Frame Buffer size

#define FILENAME    "misc/images_128.raw"
#define NPIXELS     128
#define NLINES      128
#define NIMAGES     10                  

#define INTERACTIVE 0

unsigned char buf0[NPIXELS*NLINES] __attribute__((aligned(64)));
unsigned char buf1[NPIXELS*NLINES] __attribute__((aligned(64)));

unsigned int  sts0[16]  __attribute__((aligned(64)));
unsigned int  sts1[16]  __attribute__((aligned(64)));

////////////////////////////////////////////
__attribute__((constructor)) void main()
////////////////////////////////////////////
{
    // get processor identifiers
    unsigned int    x;
    unsigned int    y; 
    unsigned int    p;
    giet_proc_xyp( &x, &y, &p );

    int             fd;
    unsigned int    image = 0;

    char            byte;

    // parameters checking
    if ( (NPIXELS != FBUF_X_SIZE) || (NLINES != FBUF_Y_SIZE) )
    {
        giet_exit("[DISPLAY ERROR] Frame buffer size does not fit image size");
    }

    // get a private TTY 
    giet_tty_alloc(0);

    giet_tty_printf("\n[DISPLAY] P[%d,%d,%d] starts at cycle %d\n"
                    "  - buf0_vaddr = %x\n" 
                    "  - buf1_vaddr = %x\n" 
                    "  - sts0_vaddr = %x\n" 
                    "  - sts1_vaddr = %x\n",
                    x, y, p, giet_proctime(),
                    buf0, buf1, sts0, sts1 );

    // open file
    fd = giet_fat_open( FILENAME , 0 );

    giet_tty_printf("\n[DISPLAY] P[%d,%d,%d] open file %s at cycle %d\n", 
                    x, y, p, FILENAME, giet_proctime() );

    // get a Chained Buffer DMA channel
    giet_fbf_cma_alloc();

    // initialize the source and destination chbufs
    giet_fbf_cma_init_buf( buf0 , buf1 , sts0 , sts1 );

    // start Chained Buffer DMA channel
    giet_fbf_cma_start( NPIXELS*NLINES );
    
    giet_tty_printf("\n[DISPLAY] Proc[%d,%d,%d] starts CMA at cycle %d\n", 
                    x, y, p, giet_proctime() );

    // Main loop on images
    while ( 1 )
    {
        // load buf0
        giet_fat_read( fd, buf0, NPIXELS*NLINES );

        giet_tty_printf("\n[DISPLAY] Proc[%d,%d,%d] load image %d at cycle %d\n", 
                        x, y, p, image, giet_proctime() );

        // display buf0
        giet_fbf_cma_display( 0 );

        giet_tty_printf("\n[DISPLAY] Proc[%d,%d,%d] display image %d at cycle %d\n", 
                        x, y, p, image, giet_proctime() );

        image++;

        if ( image == NIMAGES )
        {
            image = 0;
            giet_fat_lseek( fd , 0 , 0 );
        }

        if ( INTERACTIVE ) giet_tty_getc( &byte );

        // load buf1
        giet_fat_read( fd, buf1, NPIXELS*NLINES );

        giet_tty_printf("\n[DISPLAY] Proc[%d,%d,%d] load image %d at cycle %d\n", 
                        x, y, p, image, giet_proctime() );

        // display buf1
        giet_fbf_cma_display( 1 );

        giet_tty_printf("\n[DISPLAY] Proc[%d,%d,%d] display image %d at cycle %d\n", 
                        x, y, p, image, giet_proctime() );

        image++;

        if ( image == NIMAGES )
        {
            image = 0;
            giet_fat_lseek( fd , 0 , 0 );
        }

        if ( INTERACTIVE ) giet_tty_getc( &byte );
    }

    // stop Chained buffer DMA channel
    giet_fbf_cma_stop();

    giet_exit("display completed");
}
