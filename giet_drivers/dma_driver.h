//////////////////////////////////////////////////////////////////////////////////
// File     : dma_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
//////////////////////////////////////////////////////////////////////////////////
// The dma_driver.c and dma_driver.h files are part ot the GIET-VM nano-kernel.
// This driver supports the SoCLib vci_multi_dma component.
// 
// It can exist several DMA controlers in the architecture (one per cluster),
// and each controller can contain several channels.
// 
// There is  (NB_CLUSTERS * NB_DMA_CHANNELS) channels, indexed by a global index:
//        dma_id = cluster_xy * NB_DMA_CHANNELS + loc_id
//
// A DMA channel is a private ressource allocated to a given processor.
// It is exclusively used by the kernel to speedup data transfers, and
// there is no lock protecting exclusive access to the channel.
// As the kernel uses a polling policy on the DMA_STATUS register to detect
// transfer completion, the DMA IRQ is not used.
//
// The virtual base address of the segment associated to a channel is:
//    SEG_DMA_BASE + cluster_xy * PERI_CLUSTER_INCREMENT + DMA_SPAN * channel_id
//
// The SEG_DMA_BASE virtual address must be defined in the hard_config.h file.
//////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_DMA_DRIVER_H_
#define _GIET_DMA_DRIVER_H_

//////////////////////////////////////////////////////////////////////////////////
// Multi DMA registers offset
//////////////////////////////////////////////////////////////////////////////////

enum DMA_registers 
{
    DMA_SRC         = 0,
    DMA_DST         = 1,
    DMA_LEN         = 2,
    DMA_RESET       = 3,
    DMA_IRQ_DISABLE = 4,
    DMA_SRC_EXT     = 5,
    DMA_DST_EXT     = 6,
    /**/
    DMA_END         = 7,
    DMA_SPAN        = 8,
};

enum SoclibDmaStatus 
{
    DMA_SUCCESS      = 0,
    DMA_READ_ERROR   = 1,
    DMA_IDLE         = 2,
    DMA_WRITE_ERROR  = 3,
    DMA_BUSY         = 4,
};


//////////////////////////////////////////////////////////////////////////////////
//                  low-level access functions  
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// This function disables interrupts for one DMA channel in one cluster.
// In case of error (illegal DMA channel), an error 
// message is displayed on TTY0, and system crash.
//////////////////////////////////////////////////////////////////////////////////
extern void _dma_disable_irq( unsigned int cluster_xy,
                              unsigned int channel_id );

//////////////////////////////////////////////////////////////////////////////////
// This function re-initialises one DMA channel in one cluster after transfer
// completion. It actually forces the channel to return in IDLE state.
// In case of error (illegal DMA channel), an error 
// message is displayed on TTY0, and system crash.
//////////////////////////////////////////////////////////////////////////////////
extern void _dma_reset_channel( unsigned int  cluster_xy, 
                                unsigned int  channel_id );

//////////////////////////////////////////////////////////////////////////////////
// This function returns the status of a DMA channel in a given cluster.
// In case of error (illegal DMA channel), an error 
// message is displayed on TTY0, and system crash.
//////////////////////////////////////////////////////////////////////////////////
extern void _dma_get_status( unsigned int  cluster_xy, 
                             unsigned int  channel_id,
                             unsigned int* status );

//////////////////////////////////////////////////////////////////////////////////
// This function sets the physical address (including 64 bits extension)
// for the source and destination buffers in a DMA channel in a given cluster
// and sets the transfer size to lauch the transfer.
// In case of error (illegal DMA channel), an error 
// message is displayed on TTY0, and system crash.
//////////////////////////////////////////////////////////////////////////////////
extern void _dma_start_transfer( unsigned int       cluster_xy,
                                 unsigned int       channel_id,
                                 unsigned long long dst_paddr,
                                 unsigned long long src_paddr,
                                 unsigned int       size );

//////////////////////////////////////////////////////////////////////////////////
//                     higher level access function
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// This function copies a source memory buffer to a destination memory buffer,
// using directly physical addresses.
// This blocking function is supposed to be used by the kernel only, 
// and uses a polling policy on DMA_STATUS register to detect completion.
// Therefore, the DMA_IRQ is NOT used.
// The source and destination buffers base addresses must be word aligned, 
// and the buffer's size must be multiple of 4.
// In case of error (buffer unmapped, unaligned, or DMA_STATUS error), an error 
// message is displayed on TTY0, and system crash.
//////////////////////////////////////////////////////////////////////////////////
extern void _dma_physical_copy( unsigned int       cluster_xy,
                                unsigned int       channel_id,
                                unsigned long long dst_paddr,
                                unsigned long long src_paddr,
                                unsigned int       size ); 

//////////////////////////////////////////////////////////////////////////////////
// This function copies a source memory buffer to a destination memory buffer,
// making virtual to physical address translation: the MMU should be activated. 
// The source and destination buffers base addresses must be word aligned, 
// and the buffer's size must be multiple of 4.
// In case of error (buffer unmapped, unaligned, or DMA_STATUS error), an error 
// message is displayed on TTY0, and system crash.
//////////////////////////////////////////////////////////////////////////////////
extern void _dma_copy(  unsigned int cluster_xy,
                        unsigned int channel_id,
                        unsigned int dst_vaddr,
                        unsigned int src_vaddr,
                        unsigned int size ); 

//////////////////////////////////////////////////////////////////////////////////
// Interrupt Service Routine.
//////////////////////////////////////////////////////////////////////////////////
extern void _dma_isr( unsigned int irq_type,
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

