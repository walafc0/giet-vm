///////////////////////////////////////////////////////////////////////////
// File     : irq_handler.h
// Date     : 01/04/2012
// Author   : alain greiner 
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////
// The irq_handler.c and irq_handler.h files are part of the GIET-VM.
// They contain the code of used to handlle HWI, WTI, PTI interrupts.
///////////////////////////////////////////////////////////////////////////

#include <giet_config.h>

#ifndef _IRQ_HANDLER_H
#define _IRQ_HANDLER_H

///////////////////////////////////////////////////////////////////////////
// This enum must consistent with the values defined in 
// xml_driver.c / xml_parser.c / irq_handler.c / mapping.py 
///////////////////////////////////////////////////////////////////////////

enum isr_type_t
{
    ISR_DEFAULT = 0,
    ISR_TICK    = 1,
    ISR_TTY_RX  = 2,
    ISR_TTY_TX  = 3,
    ISR_BDV     = 4,
    ISR_TIMER   = 5,
    ISR_WAKUP   = 6,
    ISR_NIC_RX  = 7,
    ISR_NIC_TX  = 8,
    ISR_CMA     = 9,
    ISR_MMC     = 10,
    ISR_DMA     = 11,
    ISR_SDC     = 12,
    ISR_MWR     = 13,
    ISR_HBA     = 14,
};

///////////////////////////////////////////////////////////////////////////
//    Global variables allocated in irq_handler.c     
///////////////////////////////////////////////////////////////////////////

// array of external IRQ indexes for each (isr/channel) couple 
extern unsigned char _ext_irq_index[GIET_ISR_TYPE_MAX][GIET_ISR_CHANNEL_MAX];

// WTI mailbox allocators for external IRQ routing (3 allocators per proc)
extern unsigned char _wti_alloc_one[X_SIZE][Y_SIZE][NB_PROCS_MAX];
extern unsigned char _wti_alloc_two[X_SIZE][Y_SIZE][NB_PROCS_MAX];
extern unsigned char _wti_alloc_ter[X_SIZE][Y_SIZE][NB_PROCS_MAX];

// ISR type names
extern char* _isr_type_str[GIET_ISR_TYPE_MAX];

// IRQ type names
extern char* _irq_type_str[3];

///////////////////////////////////////////////////////////////////////////
//    irq_handler functions
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// This function is only used when the architecture contains an external
// IOPIC component. It initializes the _ext_irq_index[isr][channel] array,
// defining the IRQ index associated to (isr_type/isr_channel) couple.
// This array is used by the kernel for dynamic routing of an external IRQ
// signaling completion to  the processor that launched the I/O operation.
///////////////////////////////////////////////////////////////////////////

extern void _ext_irq_init();

///////////////////////////////////////////////////////////////////////////
// This function is only used when the architecture contains an external
// IOPIC component. It dynamically routes an external IRQ signaling 
// completion of an I/O operation to the processor P[x,y,p] running
// the calling task.
// 1) it allocates a WTI mailbox in the XCU of cluster[x,y] : Each processor
//    has 3  mailboxes, with index in [4*p+1, 4*p+2, 4*p+3].
// 2) it initialises the IOPIC_ADDRESS and IOPIC_MASK registers associated
//    to the (isr_type/isr_channel) couple.
// 3) it initializes the proper entry in the WTI interrupt vector associated 
//    to processor P[x,y,p].
///////////////////////////////////////////////////////////////////////////

extern void _ext_irq_alloc( unsigned int   isr_type,
                            unsigned int   isr_channel,
                            unsigned int*  wti_index );
                             
///////////////////////////////////////////////////////////////////////////
// This function is only used when the architecture contains an external
// IOPIC component. It desallocates all ressources allocated by the 
// previous _ext_irq_alloc() function to the calling processor.
// 1)  it desactivates the PIC entry associated to (isr_type/isr_channel).
// 2) it releases the WTI mailbox in the XCU of cluster[x,y].
///////////////////////////////////////////////////////////////////////////

extern void _ext_irq_release( unsigned int isr_type,
                              unsigned int isr_channel );

///////////////////////////////////////////////////////////////////////////
// This function access the XICU component (Interrupt Controler Unit)
// to get the interrupt vector entry. There is one XICU component per
// cluster, and this component can support up to NB_PROCS_MAX output IRQs.
// It returns the highest priority active interrupt index (smaller
// indexes have the highest priority).
// Any value larger than 31 means "no active interrupt".
//
// There is three interrupt vectors per processor (stored in the processor's
// scheduler) for the three HWI, PTI, and WTI interrupts types.
// Each interrupt vector entry contains two fields:
// - isr_type     bits[15:0]  : defines the type of ISR to be executed.
// - isr_channel  bits[31:16] : defines the channel index 
// If the peripheral is replicated in clusters, the channel_id is 
// a global index : channel_id = cluster_id * NB_CHANNELS_MAX + loc_id   
///////////////////////////////////////////////////////////////////////////

extern void _irq_demux();

///////////////////////////////////////////////////////////////////////////
// This default ISR is called  when the interrupt handler is called, 
// and there is no active IRQ. It displays a warning message on TTY[0].
///////////////////////////////////////////////////////////////////////////

extern void _isr_default();

///////////////////////////////////////////////////////////////////////////
// This ISR can only be executed after a WTI to force a context switch
// on a remote processor. The context switch is only executed if the 
// current task is the IDLE_TASK, or if the value written in the mailbox 
// is non zero.
///////////////////////////////////////////////////////////////////////////

extern void _isr_wakup( unsigned int irq_type,
                        unsigned int irq_id,
                        unsigned int channel );

/////////////////////////////////////////////////////////////////////////////////////
// This ISR is in charge of context switch, and handles the IRQs generated by
// the "system" timers. It can be PTI in case of XCU, or it can be HWI generated
// by an external timer in case of ICU.
// The ISR acknowledges the IRQ, and calls the _ctx_switch() function.
/////////////////////////////////////////////////////////////////////////////////////
extern void _isr_tick( unsigned int irq_type,
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

