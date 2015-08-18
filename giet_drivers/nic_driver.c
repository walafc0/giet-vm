///////////////////////////////////////////////////////////////////////////////////
// File     : nic_driver.c
// Date     : 23/05/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <nic_driver.h>
#include <cma_driver.h>
#include <utils.h>
#include <tty0.h>
#include <ctx_handler.h>
#include <vmem.h>

#if !defined(GIET_USE_IOMMU) 
# error: You must define GIET_USE_IOMMU in the giet_config.h file
#endif

#if !defined(SEG_NIC_BASE)
# error: You must define SEG_NIC_BASE in the hard_config.h file 
#endif

#if !defined(NB_NIC_CHANNELS)
# error: You must define NB_NIC_CHANNELS in the hard_config.h file 
#endif

#if !defined(X_IO)
# error: You must define X_IO in the hard_config.h file
#endif

#if !defined(Y_IO)
# error: You must define Y_IO in the hard_config.h file
#endif

#if ( NB_NIC_CHANNELS > 8 )
# error: NB_NIC_CHANNELS cannot be larger than 8
#endif

#if !defined(NB_CMA_CHANNELS)
# error: You must define NB_CMA_CHANNELS in the hard_config.h file 
#endif

#if ( NB_CMA_CHANNELS > 8 )
# error: NB_CMA_CHANNELS cannot be larger than 8
#endif

#if !defined( USE_IOB )
# error: You must define USE_IOB in the hard_config.h file
#endif

///////////////////////////////////////////////////////////////////////////////
// This low_level function returns the value contained in a channel register. 
///////////////////////////////////////////////////////////////////////////////
unsigned int _nic_get_channel_register( unsigned int channel,
                                        unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_NIC_BASE + 
                           NIC_CHANNEL_SPAN * channel + 0x1000 + index;
    return _io_extended_read( vaddr );
}

///////////////////////////////////////////////////////////////////////////////
// This low-level function set a new value in a channel register.
///////////////////////////////////////////////////////////////////////////////
void _nic_set_channel_register( unsigned int channel,
                                unsigned int index,
                                unsigned int value ) 
{
    unsigned int* vaddr = (unsigned int*)SEG_NIC_BASE + 
                           NIC_CHANNEL_SPAN * channel + 0x1000 + index;
    _io_extended_write( vaddr, value );
}

///////////////////////////////////////////////////////////////////////////////
// This low_level function returns the value contained in a global register. 
///////////////////////////////////////////////////////////////////////////////
unsigned int _nic_get_global_register( unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_NIC_BASE + 
                           NIC_CHANNEL_SPAN * 8 + index;
    return _io_extended_read( vaddr );
}

///////////////////////////////////////////////////////////////////////////////
// This low-level function set a new value in a global register.
///////////////////////////////////////////////////////////////////////////////
void _nic_set_global_register( unsigned int index,
                               unsigned int value ) 
{
    unsigned int* vaddr = (unsigned int*)SEG_NIC_BASE + 
                           NIC_CHANNEL_SPAN * 8 + index;
    _io_extended_write( vaddr, value );
}

////////////////////////////////////////////
int _nic_global_init( unsigned int bc_enable,
                      unsigned int bypass_enable,
                      unsigned int tdm_enable,
                      unsigned int tdm_period )
{
    _nic_set_global_register( NIC_G_BC_ENABLE    , bc_enable );
    _nic_set_global_register( NIC_G_BYPASS_ENABLE, bypass_enable );
    _nic_set_global_register( NIC_G_TDM_ENABLE   , tdm_enable );
    _nic_set_global_register( NIC_G_TDM_PERIOD   , tdm_period );
    _nic_set_global_register( NIC_G_VIS          , 0 );     // channels activated later
    _nic_set_global_register( NIC_G_ON           , 1 );   

    return 0;
}

////////////////////////////////////////////
int _nic_channel_start( unsigned int channel,
                        unsigned int is_rx,
                        unsigned int mac4,
                        unsigned int mac2 )
{
    unsigned int vis = _nic_get_global_register( NIC_G_VIS );
    vis |= (0x1 << channel );

    _nic_set_global_register( NIC_G_MAC_4 + channel, mac4 );
    _nic_set_global_register( NIC_G_MAC_2 + channel, mac2 );
    _nic_set_global_register( NIC_G_VIS            , vis );
    
    unsigned int base     = SEG_NIC_BASE;
    unsigned int extend   = (X_IO << Y_WIDTH) + Y_IO;

    unsigned int buf_0_addr;
    unsigned int buf_1_addr;
    unsigned int sts_0_addr;
    unsigned int sts_1_addr;

    unsigned int desc_lo_0;
    unsigned int desc_lo_1;
    unsigned int desc_hi_0;
    unsigned int desc_hi_1;

    if ( is_rx )
    {
        buf_0_addr = base;
        buf_1_addr = base + 0x1000;
        sts_0_addr = base + 0x4000;
        sts_1_addr = base + 0x4040;

        desc_lo_0 = (sts_0_addr >> 6) + ((buf_0_addr & 0xFC0) << 20);
        desc_lo_1 = (sts_1_addr >> 6) + ((buf_1_addr & 0xFC0) << 20);
        desc_hi_0 = ((buf_0_addr & 0xFFFFF000) >> 12) + ((extend & 0xFFF) << 20);
        desc_hi_1 = ((buf_1_addr & 0xFFFFF000) >> 12) + ((extend & 0xFFF) << 20);

        _nic_set_channel_register( channel, NIC_RX_DESC_LO_0, desc_lo_0     );
        _nic_set_channel_register( channel, NIC_RX_DESC_LO_1, desc_lo_1     );
        _nic_set_channel_register( channel, NIC_RX_DESC_HI_0, desc_hi_0     );
        _nic_set_channel_register( channel, NIC_RX_DESC_HI_1, desc_hi_1     );
        _nic_set_channel_register( channel, NIC_RX_RUN      , 1             );
    }
    else
    {
        buf_0_addr = base + 0x2000;
        buf_1_addr = base + 0x3000;
        sts_0_addr = base + 0x4080;
        sts_1_addr = base + 0x40c0;

        desc_lo_0 = (sts_0_addr >> 6) + ((buf_0_addr & 0xFC0) << 20);
        desc_lo_1 = (sts_1_addr >> 6) + ((buf_1_addr & 0xFC0) << 20);
        desc_hi_0 = ((buf_0_addr & 0xFFFFF000) >> 12) + ((extend & 0xFFF) << 20);
        desc_hi_1 = ((buf_1_addr & 0xFFFFF000) >> 12) + ((extend & 0xFFF) << 20);

        _nic_set_channel_register( channel, NIC_TX_DESC_LO_0, desc_lo_0     );
        _nic_set_channel_register( channel, NIC_TX_DESC_LO_1, desc_lo_1     );
        _nic_set_channel_register( channel, NIC_TX_DESC_HI_0, desc_hi_0     );
        _nic_set_channel_register( channel, NIC_TX_DESC_HI_1, desc_hi_1     );
        _nic_set_channel_register( channel, NIC_TX_RUN      , 1             );
    }

    return 0;
}

////////////////////////////////////////////
int _nic_channel_stop( unsigned int channel,
                       unsigned int is_rx )
{
    if ( is_rx )  _nic_set_channel_register( channel, NIC_RX_RUN, 0 );
    else          _nic_set_channel_register( channel, NIC_TX_RUN, 0 );

    return 0;   
}



////////////////////////////////////////////////////////////////////////////////////////////
//            Interrupt Service Routines
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////
void _nic_rx_isr( unsigned int irq_type,
                  unsigned int irq_id,
                  unsigned int channel )
{
    _puts("[NIC WARNING] RX buffers are full for NIC channel ");
    _putd( channel );
    _puts("\n");
}

////////////////////////////////////////
void _nic_tx_isr( unsigned int irq_type,
                  unsigned int irq_id,
                  unsigned int channel )
{
    _puts("[NIC WARNING] TX buffers are full for NIC channel ");
    _putd( channel );
    _puts("\n");
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

