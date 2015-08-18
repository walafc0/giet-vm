///////////////////////////////////////////////////////////////////////////////////
// File     : pic_driver.c
// Date     : 05/03/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <pic_driver.h>
#include <giet_config.h>
#include <hard_config.h>
#include <utils.h>

#if !defined(SEG_PIC_BASE) 
# error: You must define SEG_PIC_BASE in the hard_config.h file
#endif

/////////////////////////////////////////////////////
unsigned int _pic_get_register( unsigned int channel,
                                unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_PIC_BASE + channel*IOPIC_SPAN + index;
    return _io_extended_read( vaddr );
}

/////////////////////////////////////////////
void _pic_set_register( unsigned int channel,
                        unsigned int index,
                        unsigned int value )
{
    unsigned int* vaddr = (unsigned int*)SEG_PIC_BASE + channel*IOPIC_SPAN + index;
    _io_extended_write( vaddr, value );
}


/////////////////////////////////////
void _pic_init( unsigned int channel,      // source PIC HWI channel
                unsigned int vaddr,        // dest XCU WTI address
                unsigned int extend )      // dest XCU cluster_xy
{
    _pic_set_register( channel, IOPIC_ADDRESS, vaddr );
    _pic_set_register( channel, IOPIC_EXTEND, extend );
    _pic_set_register( channel, IOPIC_MASK, 1 );
}

////////////////////////////////////////////////////
unsigned int _pic_get_status( unsigned int channel )
{
    return _pic_get_register( channel, IOPIC_STATUS );
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

