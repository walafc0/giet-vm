///////////////////////////////////////////////////////////////////////////////////
// File     : xcu_driver.c
// Date     : 23/05/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <hard_config.h>
#include <giet_config.h>
#include <xcu_driver.h>
#include <tty0.h>
#include <mapping_info.h>
#include <utils.h>
#include <io.h>

#if !defined(X_SIZE) 
# error: You must define X_SIZE in the hard_config.h file
#endif

#if !defined(Y_SIZE) 
# error: You must define X_SIZE in the hard_config.h file
#endif

#if !defined(X_WIDTH) 
# error: You must define X_WIDTH in the hard_config.h file
#endif

#if !defined(Y_WIDTH) 
# error: You must define X_WIDTH in the hard_config.h file
#endif

#if !defined(NB_PROCS_MAX) 
# error: You must define NB_PROCS_MAX in the hard_config.h file
#endif

#if !defined( USE_XCU )
# error: You must define USE_XCU in the hard_config.h file
#endif

#if !defined( SEG_XCU_BASE )
# error: You must define SEG_XCU_BASE in the hard_config.h file
#endif

#if !defined( PERI_CLUSTER_INCREMENT )
# error: You must define PERI_CLUSTER_INCREMENT in the hard_config.h file
#endif

///////////////////////////////////////////////////////////////////////////////
// This low level function returns the value contained in register "index"
// in the XCU component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////
static
unsigned int _xcu_get_register( unsigned int cluster_xy, // cluster index
                                unsigned int func,       // function index
                                unsigned int index )     // register index
{
    unsigned int vaddr =
        SEG_XCU_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        (XCU_REG(func, index) << 2);

    return ioread32( (void*)vaddr );
}

///////////////////////////////////////////////////////////////////////////////
// This low level function sets a new value in register "index"
// in the XCU component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////
static
void _xcu_set_register( unsigned int cluster_xy,       // cluster index
                        unsigned int func,             // func index
                        unsigned int index,            // register index
                        unsigned int value )           // value to be written
{
    unsigned int vaddr =
        SEG_XCU_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        (XCU_REG(func, index) << 2);
        
    iowrite32( (void*)vaddr, value );
}

////////////////////////////////////////////
void _xcu_set_mask( unsigned int cluster_xy, 
                    unsigned int channel,  
                    unsigned int value,
                    unsigned int irq_type ) 
{
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)                                   _exit(); 
    if (y >= Y_SIZE)                                   _exit(); 
    if (channel >= (NB_PROCS_MAX * IRQ_PER_PROCESSOR)) _exit(); 

    unsigned int func = 0;
    if      (irq_type == IRQ_TYPE_PTI) func = XCU_MSK_PTI_ENABLE;
    else if (irq_type == IRQ_TYPE_WTI) func = XCU_MSK_WTI_ENABLE;
    else if (irq_type == IRQ_TYPE_HWI) func = XCU_MSK_HWI_ENABLE;
    else
    { 
        _printf("[GIET ERROR] _xcu_set_mask() receives illegal IRQ type\n");
        _exit();
    }

    _xcu_set_register(cluster_xy, func, channel, value);
}

/////////////////////////////////////////////
void _xcu_get_index( unsigned int cluster_xy, 
                     unsigned int channel,   
                     unsigned int * index, 
                     unsigned int * irq_type )
{
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)                                   _exit(); 
    if (y >= Y_SIZE)                                   _exit(); 
    if (channel >= (NB_PROCS_MAX * IRQ_PER_PROCESSOR)) _exit(); 

    unsigned int prio = _xcu_get_register(cluster_xy, XCU_PRIO, channel);
    unsigned int pti_ok = (prio & 0x00000001);
    unsigned int hwi_ok = (prio & 0x00000002);
    unsigned int wti_ok = (prio & 0x00000004);
    unsigned int pti_id = (prio & 0x00001F00) >> 8;
    unsigned int hwi_id = (prio & 0x001F0000) >> 16;
    unsigned int wti_id = (prio & 0x1F000000) >> 24;
    if      (pti_ok)
    {
        *index    = pti_id;
        *irq_type = IRQ_TYPE_PTI;
    }
    else if (hwi_ok)
    {
        *index    = hwi_id;
        *irq_type = IRQ_TYPE_HWI;
    }
    else if (wti_ok) 
    {
        *index    = wti_id;
        *irq_type = IRQ_TYPE_WTI;
    }
    else 
    {
        *index = 32;
    }
}

////////////////////////////////////////////
void _xcu_send_wti( unsigned int cluster_xy,
                    unsigned int wti_index,
                    unsigned int wdata )
{ 
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)               _exit(); 
    if (y >= Y_SIZE)               _exit(); 
    if (wti_index >= 32)           _exit(); 

    _xcu_set_register(cluster_xy, XCU_WTI_REG, wti_index, wdata);
} 

////////////////////////////////////////////
void _xcu_send_wti_paddr( unsigned int cluster_xy,
                          unsigned int wti_index,
                          unsigned int wdata )
{ 
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)               _exit(); 
    if (y >= Y_SIZE)               _exit(); 
    if (wti_index >= 32)           _exit(); 

    paddr_t paddr =
         SEG_XCU_BASE + ((paddr_t)cluster_xy << 32) + 
        (XCU_REG(XCU_WTI_REG, wti_index) << 2);

    _physical_write(paddr, wdata);
}

///////////////////////////////////////////////////
void _xcu_get_wti_value( unsigned int   cluster_xy,
                         unsigned int   wti_index,
                         unsigned int * value )
{
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)               _exit(); 
    if (y >= Y_SIZE)               _exit(); 
    if (wti_index >= 32)           _exit(); 
 
    *value = _xcu_get_register(cluster_xy, XCU_WTI_REG, wti_index);
}

////////////////////////////////////////////////////
void _xcu_get_wti_address( unsigned int   wti_index,
                           unsigned int * address )
{
    if (wti_index >= 32)  _exit(); 
 
    *address = SEG_XCU_BASE + (XCU_REG(XCU_WTI_REG, wti_index)<<2); 
}

///////////////////////////////////////////////
void _xcu_timer_start( unsigned int cluster_xy,
                       unsigned int pti_index,
                       unsigned int period )
{
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)             _exit(); 
    if (y >= Y_SIZE)             _exit(); 

    _xcu_set_register(cluster_xy, XCU_PTI_PER, pti_index, period);
}

//////////////////////////////////////////////
void _xcu_timer_stop( unsigned int cluster_xy, 
                      unsigned int pti_index) 
{
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)             _exit(); 
    if (y >= Y_SIZE)             _exit(); 

    _xcu_set_register(cluster_xy, XCU_PTI_PER, pti_index, 0);
}

///////////////////////////////////////////////////////////
void _xcu_timer_reset_irq( unsigned int cluster_xy, 
                           unsigned int pti_index ) 
{
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)             _exit(); 
    if (y >= Y_SIZE)             _exit(); 

    // This return value is not used / avoid a compilation warning.
    x = _xcu_get_register(cluster_xy, XCU_PTI_ACK, pti_index);
}

///////////////////////////////////////////////////
void _xcu_timer_reset_cpt( unsigned int cluster_xy, 
                           unsigned int pti_index ) 
{
    // parameters checking 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if (x >= X_SIZE)             _exit(); 
    if (y >= Y_SIZE)             _exit(); 

    unsigned int per = _xcu_get_register(cluster_xy, XCU_PTI_PER, pti_index);

    // we write 0 first because if the timer is currently running, 
    // the corresponding timer counter is not reset
    _xcu_set_register(cluster_xy, XCU_PTI_PER, pti_index, 0);
    _xcu_set_register(cluster_xy, XCU_PTI_PER, pti_index, per);
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

