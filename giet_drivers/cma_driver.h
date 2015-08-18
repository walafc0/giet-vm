///////////////////////////////////////////////////////////////////////////////////
// File     : cma_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The cma_driver.c and cma_driver.h files are part ot the GIET-VM kernel.
// This driver supports the SocLib vci_chbuf_dma component, that is
// a multi channels, chained buffer DMA controller.
//
// This component can be used in conjonction with the SocLib vci_frame_buffer 
// to display images, or with the SocLib vci_multi_nic controller to tranfer
// RX or TX packets between NIC and memory buffers.
//
// The SEG_CMA_BASE address must be defined in the hard_config.h file
//
// All accesses to CMA registers are done by the two _cma_set_register() 
// and _cma_get_register() low-level functions, that are handling virtual 
// to physical extended addressing.
//
// The higher level access functions are defined in the fbf_driver 
// and nic_driver files.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_CMA_DRIVERS_H_
#define _GIET_CMA_DRIVERS_H_

///////////////////////////////////////////////////////////////////////////////////
//  CMA channel registers offsets
///////////////////////////////////////////////////////////////////////////////////

enum CMA_registers_e 
{
    CHBUF_RUN           = 0,    // write-only : channel activated
    CHBUF_STATUS        = 1,    // read-only  : channel fsm state
    CHBUF_SRC_DESC      = 2,    // read/write : source chbuf pbase address
    CHBUF_DST_DESC      = 3,    // read/write : dest chbuf pbase address
    CHBUF_SRC_NBUFS     = 4,    // read/write : source chbuf number of buffers
    CHBUF_DST_NBUFS     = 5,    // read/write : dest chbuf number of buffers
    CHBUF_BUF_SIZE      = 6,    // read/write : buffer size for source & dest  
    CHBUF_PERIOD        = 7,    // read/write : period for status polling 
    CHBUF_SRC_EXT       = 8,    // read/write : source chbuf pbase extension
    CHBUF_DST_EXT       = 9,    // read/write : dest chbuf pbase extension
    /****/
    CHBUF_CHANNEL_SPAN	= 1024,
};

///////////////////////////////////////////////////////////////////////////////////
//  CMA channel status values
///////////////////////////////////////////////////////////////////////////////////

enum CMA_status_e
{
    CHANNEL_IDLE,

    CHANNEL_DATA_ERROR,
    CHANNEL_SRC_DESC_ERROR,
    CHANNEL_DST_DESC_ERROR,
    CHANNEL_SRC_STATUS_ERROR,
    CHANNEL_DST_STATUS_ERROR,

    CHANNEL_READ_SRC_DESC,
    CHANNEL_READ_SRC_DESC_WAIT,
    CHANNEL_READ_SRC_STATUS,
    CHANNEL_READ_SRC_STATUS_WAIT,
    CHANNEL_READ_SRC_STATUS_DELAY,

    CHANNEL_READ_DST_DESC,
    CHANNEL_READ_DST_DESC_WAIT,
    CHANNEL_READ_DST_STATUS,
    CHANNEL_READ_DST_STATUS_WAIT,
    CHANNEL_READ_DST_STATUS_DELAY,

    CHANNEL_BURST,
    CHANNEL_READ_REQ,
    CHANNEL_READ_REQ_WAIT,
    CHANNEL_RSP_WAIT,
    CHANNEL_WRITE_REQ,
    CHANNEL_WRITE_REQ_WAIT,
    CHANNEL_BURST_RSP_WAIT,

    CHANNEL_SRC_STATUS_WRITE,
    CHANNEL_SRC_STATUS_WRITE_WAIT,
    CHANNEL_DST_STATUS_WRITE,
    CHANNEL_DST_STATUS_WRITE_WAIT,
    CHANNEL_NEXT_BUFFERS,
};

///////////////////////////////////////////////////////////////////////////////////
//    access functions
///////////////////////////////////////////////////////////////////////////////////

extern unsigned int _cma_get_register( unsigned int channel,
                                       unsigned int index );

extern void _cma_set_register( unsigned int channel,
                               unsigned int index,
                               unsigned int value );

extern void _cma_isr( unsigned int irq_type,
                      unsigned int irq_id,
                      unsigned int channel );

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

