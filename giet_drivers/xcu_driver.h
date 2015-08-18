///////////////////////////////////////////////////////////////////////////////////
// File     : xcu_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The xcu_driver.c and xcu_driver.h files are part ot the GIET-VM nano-kernel.
// This driver supports the SoCLib vci_xicu, that is a vectorised interrupt
// controler supporting three types of interrupts:
//
// - HWI : HardWare Interrupts (from hardware peripherals)
// - PTI : Programmable Timer Interrupts (contained in the XCU component)
// - WTI : Write Trigered Interrupts (from software, or from a PIC controller) 
//
// It can exist several interrupt controller unit in the architecture 
// (one per cluster), and each one can contain several channels:
// The number of XICU channels is equal to NB_PROCS_MAX, because there is 
// one private XICU channel per processor in a cluster.
//
// The virtual base address of the segment associated to the component is:
//
//      vbase = SEG_XCU_BASE + cluster_xy * PERI_CLUSTER_INCREMENT
//
// The SEG_XCU_BSE  and PERI_CLUSTER_INCREMENT values must be defined 
// in the hard_config.h file.
////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_XCU_DRIVER_H_
#define _GIET_XCU_DRIVER_H_

///////////////////////////////////////////////////////////////////////////////////
// XICU registers offsets
///////////////////////////////////////////////////////////////////////////////////

enum Xcu_registers 
{
    XCU_WTI_REG = 0,
    XCU_PTI_PER = 1,
    XCU_PTI_VAL = 2,
    XCU_PTI_ACK = 3,

    XCU_MSK_PTI = 4,
    XCU_MSK_PTI_ENABLE = 5,
    XCU_MSK_PTI_DISABLE = 6,
    XCU_PTI_ACTIVE = 6,

    XCU_MSK_HWI = 8,
    XCU_MSK_HWI_ENABLE = 9,
    XCU_MSK_HWI_DISABLE = 10,
    XCU_HWI_ACTIVE = 10,

    XCU_MSK_WTI = 12,
    XCU_MSK_WTI_ENABLE = 13,
    XCU_MSK_WTI_DISABLE = 14,
    XCU_WTI_ACTIVE = 14,

    XCU_PRIO = 15,
};

#define XCU_REG(func, index) (((func)<<5)|(index))
 
////////////////////////////////////////////////////////////////////////////////
//                           access functions 
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// This function set the mask register for the IRQ type defined by "irq_type",
// and for the channel identified by the "cluster_xy" and "channel" arguments.
// All '1' bits are set / all '0' bits are not modified.
////////////////////////////////////////////////////////////////////////////////
extern void _xcu_set_mask( unsigned int cluster_xy,
                           unsigned int channel,  
                           unsigned int mask, 
                           unsigned int irq_type );

////////////////////////////////////////////////////////////////////////////////
// This function returns the index and the type of the highest priority 
// - active PTI (Timer Interrupt), then
// - active HWI (Hardware Interrupt), then
// - active WTI (Software Interrupt)
////////////////////////////////////////////////////////////////////////////////
extern void _xcu_get_index( unsigned int   cluster_xy, 
                            unsigned int   channel,   
                            unsigned int * index,
                            unsigned int * irq_type );

////////////////////////////////////////////////////////////////////////////////
// This function writes the "wdata" value in the mailbox defined 
// by the "cluster_xy" and "wti_index" arguments.
////////////////////////////////////////////////////////////////////////////////
extern void _xcu_send_wti( unsigned int cluster_xy,
                           unsigned int wti_index,
                           unsigned int wdata );

///////////////////////////////////////////////////////////////////////////////
// This function writes the "wdata" by physical xcu address value in the mailbox 
// defined by the "cluster_xy" and "wti_index" arguments.
////////////////////////////////////////////////////////////////////////////////
void _xcu_send_wti_paddr( unsigned int cluster_xy,
                          unsigned int wti_index,
                          unsigned int wdata );
////////////////////////////////////////////////////////////////////////////////
// This function returns the value contained in a WTI mailbox defined by
// the cluster_xy and "wti_index" arguments. This value is written in
// the "value" argument, and the corresponding WTI is acknowledged.
////////////////////////////////////////////////////////////////////////////////
extern void _xcu_get_wti_value( unsigned int   cluster_xy,
                                unsigned int   wti_index,
                                unsigned int * value );

////////////////////////////////////////////////////////////////////////////////
// This function returns the address of a WTI mailbox defined by
// the "wti_index" argument, in the unsigned int "address" argument.
// It is used by the GIET to configurate the IOPIC component.
// There is no access to a specific XCU component in a specific cluster.
////////////////////////////////////////////////////////////////////////////////
extern void _xcu_get_wti_address( unsigned int   wti_index,
                                  unsigned int * address );

////////////////////////////////////////////////////////////////////////////////
// This function activates a timer contained in XCU by writing in the
// proper register the period value.
////////////////////////////////////////////////////////////////////////////////
extern void _xcu_timer_start( unsigned int cluster_xy, 
                              unsigned int pti_index,
                              unsigned int period ); 

//////////////////////////////////////////////////////////////////////////////
// This function desactivates a timer in XCU component
// by writing in the proper register.
//////////////////////////////////////////////////////////////////////////////
extern void _xcu_timer_stop( unsigned int cluster_xy, 
                             unsigned int pti_index ); 

//////////////////////////////////////////////////////////////////////////////
// This function acknowlegge a timer interrupt in XCU 
// component by reading in the proper XCU register.
// It can be used by both the isr_switch() for a "system" timer, 
// or by the _isr_timer() for an "user" timer.
//////////////////////////////////////////////////////////////////////////////
extern void _xcu_timer_reset_irq( unsigned int cluster_xy, 
                                  unsigned int pti_index );

//////////////////////////////////////////////////////////////////////////////
// This function resets a timer counter. To do so, we re-write the period
// in the proper register, what causes the count to restart.
// The period value is read from the same (TIMER_PERIOD) register,
// this is why in appearance we do nothing useful (read a value
// from a register and write this value in the same register).
// This function is called during a context switch (user or preemptive)
/////////////////////////////////////////////////////////////////////////////
extern void _xcu_timer_reset_cpt( unsigned int cluster_xy, 
                                  unsigned int pti_index ); 

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

