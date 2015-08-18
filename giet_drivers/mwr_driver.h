///////////////////////////////////////////////////////////////////////////////////
// File     : mwr_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////
// The mwr_driver.c and mwr_driver.h files are part ot the GIET-VM OS.
// This driver supports the SoCLib vci_mwmr_dma component.
// 
// It can exist several MWMR_DMA controlers in the architecture 
// (one per cluster), and each controller can contain several ports. 
// Each port defines a TO_COPROC or FROM_COPROC communication channel.
// Each channel can be configured to run in one of three predefined modes
// (MWMR / DMA_IRQ / DMA_NO_IRQ).
// The MWMR_DMA controler is a private ressource, allocated to a given task.
//
// The virtual base address of the segment associated to the coprocessor
// addressable registers is:
//   SEG_MWR_BASE + cluster_xy*PERI_CLUSTER_INCREMENT
//
// The virtual base address of the segments associated to a channel is:
//   SEG_MWR_BASE +
//   cluster_xy * PERI_CLUSTER_INCREMENT +
//   4 * CHANNEL_SPAN * (channel+1)
//
// The SEG_MWR_BASE virtual address mus be defined in the hard_config.h file.
///////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_MWR_DRIVERS_H_
#define _GIET_MWR_DRIVERS_H_

///////////////////////////////////////////////////////////////////////////////
// MWMR controler registers offsets 
///////////////////////////////////////////////////////////////////////////////

enum MwmrDmaRegisters 
{
    MWR_CHANNEL_BUFFER_LSB   = 0,     // Data buffer paddr 32 LSB bits
    MWR_CHANNEL_BUFFER_MSB   = 1,     // Data buffer paddr extension
    MWR_CHANNEL_MWMR_LSB     = 2,     // MWMR descriptor paddr 32 LSB bits
    MWR_CHANNEL_MWMR_MSB     = 3,     // MWMR descriptor paddr extension
    MWR_CHANNEL_LOCK_LSB     = 4,     // MWMR lock paddr 32 LSB bits
    MWR_CHANNEL_LOCK_MSB     = 5,     // MWMR lock paddr extension
    MWR_CHANNEL_WAY          = 6,     // TO_COPROC / FROMCOPROC         (Read-only)
    MWR_CHANNEL_MODE         = 7,     // MWMR / DMA / DMA_IRQ 
    MWR_CHANNEL_SIZE         = 8,     // Data Buffer size (bytes)
    MWR_CHANNEL_RUNNING      = 9,     // channel running   
    MWR_CHANNEL_STATUS       = 10,    // channel FSM state              (Read-Only)
    MWR_CHANNEL_INFO         = 11,    // STS | CFG | FROM | TO          (Read-Only)
    //
    MWR_CHANNEL_SPAN         = 16,
};

enum MwrDmaStatus
{
    MWR_CHANNEL_SUCCESS    = 0,
    MWR_CHANNEL_ERROR_DATA = 1,
    MWR_CHANNEL_ERROR_LOCK = 2,
    MWR_CHANNEL_ERROR_DESC = 3,
    MWR_CHANNEL_BUSY       = 4,
};

///////////////////////////////////////////////////////////////////////////////
//           Low-level access functions
///////////////////////////////////////////////////////////////////////////////

extern void _mwr_set_channel_register( unsigned int cluster_xy,
                                       unsigned int channel,
                                       unsigned int index,
                                       unsigned int value );

extern unsigned int _mwr_get_channel_register( unsigned int cluster_xy,
                                               unsigned int channel,
                                               unsigned int index );

extern void _mwr_set_coproc_register( unsigned int cluster_xy,
                                      unsigned int index,
                                      unsigned int value );

extern unsigned int _mwr_get_coproc_register( unsigned int cluster_xy,
                                              unsigned int index );

///////////////////////////////////////////////////////////////////////////////
//            Interrupt Service Routine
///////////////////////////////////////////////////////////////////////////////

extern void _mwr_isr( unsigned int irq_type,
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

