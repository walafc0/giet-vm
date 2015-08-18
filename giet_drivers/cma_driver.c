///////////////////////////////////////////////////////////////////////////////////
// File      : cma_driver.c
// Date      : 01/03/2014
// Author    : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <cma_driver.h>
#include <hard_config.h>         
#include <utils.h>
#include <tty0.h>

#if !defined(SEG_CMA_BASE) 
# error: You must define SEG_CMA_BASE in the hard_config.h file
#endif

/////////////////////////////////////////////////////
unsigned int _cma_get_register( unsigned int channel,
                                unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_CMA_BASE + 
                           CHBUF_CHANNEL_SPAN*channel + index;
    return _io_extended_read( vaddr );
}

/////////////////////////////////////////////
void _cma_set_register( unsigned int channel,
                        unsigned int index,
                        unsigned int value ) 
{
    unsigned int* vaddr = (unsigned int*)SEG_CMA_BASE + 
                           CHBUF_CHANNEL_SPAN*channel + index;
    _io_extended_write( vaddr, value );
}


//////////////////////////////////////
void _cma_isr( unsigned int irq_type,
               unsigned int irq_id,
               unsigned int channel )
{
    // get CMA channel status
    unsigned int status = _cma_get_register( channel, CHBUF_STATUS );

    _puts("\n[CMA WARNING] IRQ received for CMA channel ");
    _putd( channel );
    _puts(" blocked at cycle ");
    _putd( _get_proctime() );
    _puts("\nreset the CMA channel : ");

    if (status == CHANNEL_SRC_DESC_ERROR )
        _puts("impossible access to source chbuf descriptor\n");

    else if (status == CHANNEL_SRC_STATUS_ERROR )
        _puts("impossible access to source buffer status\n");

    else if (status == CHANNEL_DST_DESC_ERROR )
        _puts("impossible access to destination chbuf descriptor\n");

    else if (status == CHANNEL_DST_STATUS_ERROR )
        _puts("impossible access to destination buffer status\n");

    else if (status == CHANNEL_DATA_ERROR )
        _puts("impossible access to source or destination data buffer\n");

    else
        _puts("strange, because channel is not blocked...");
    
    // acknowledge IRQ and desactivates channel
    _cma_set_register( channel, CHBUF_RUN, 0 );
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

