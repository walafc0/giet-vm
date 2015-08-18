///////////////////////////////////////////////////////////////////////////////////
// File     : mwr_driver.c
// Date     : 27/02/2015
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <mapping_info.h>
#include <mwr_driver.h>
#include <xcu_driver.h>
#include <ctx_handler.h>
#include <utils.h>
#include <kernel_locks.h>
#include <tty0.h>
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

#if !defined(SEG_MWR_BASE)
# error: You must define SEG_MWR_BASE in the hard_config.h file
#endif

#if !defined(PERI_CLUSTER_INCREMENT) 
# error: You must define PERI_CLUSTER_INCREMENT in the hard_config.h file
#endif

/////////////////////////////////////////////////////////////////////////////
//      Extern variables
/////////////////////////////////////////////////////////////////////////////

// allocated in the boot.c or kernel_init.c files
extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX]; 

/////////////////////////////////////////////////////////////////////////////
//      Global variables 
/////////////////////////////////////////////////////////////////////////////
// All arrays are indexed by the cluster index.
/////////////////////////////////////////////////////////////////////////////

// Lock protecting exclusive access
__attribute__((section(".kdata")))
simple_lock_t  _coproc_lock[X_SIZE*Y_SIZE];

// Coprocessor type
__attribute__((section(".kdata")))
unsigned int  _coproc_type[X_SIZE*Y_SIZE];

// coprocessor characteristics
__attribute__((section(".kdata")))
unsigned int   _coproc_info[X_SIZE*Y_SIZE];

// Coprocessor running mode
__attribute__((section(".kdata")))
unsigned int  _coproc_mode[X_SIZE*Y_SIZE];

// coprocessor status (for MODE_DMA_IRQ)
__attribute__((section(".kdata")))
unsigned int   _coproc_error[X_SIZE*Y_SIZE];

// descheduled task gtid (for MODE_DMA_IRQ)
__attribute__((section(".kdata")))
unsigned int   _coproc_gtid[X_SIZE*Y_SIZE];

/////////////////////////////////////////////////////////////////////////////
// This function returns the value contained in a private register 
// of the coprocessor contained in cluster "cluster_xy".
/////////////////////////////////////////////////////////////////////////////
unsigned int _mwr_get_coproc_register( unsigned int cluster_xy, // cluster
                                       unsigned int index )     // register 
{
    unsigned int vaddr =
        SEG_MWR_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) + 
        (index << 2);

    return ioread32( (void*)vaddr );
}

/////////////////////////////////////////////////////////////////////////////
// This function sets a new value in a private register 
// of the coprocessor contained in cluster "clustenr_xy".
/////////////////////////////////////////////////////////////////////////////
void _mwr_set_coproc_register( unsigned int cluster_xy,   // cluster index
                               unsigned int index,        // register index
                               unsigned int value )       // writte value
{
    unsigned int vaddr =
        SEG_MWR_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        (index << 2);
        
    iowrite32( (void*)vaddr, value );
}

/////////////////////////////////////////////////////////////////////////////
// This function returns the value contained in a channel register 
// defined by the "index" and "channel" arguments, in the MWMR_DMA component
// contained in cluster "cluster_xy".
/////////////////////////////////////////////////////////////////////////////
unsigned int _mwr_get_channel_register( unsigned int cluster_xy, // cluster 
                                        unsigned int channel,    // channel 
                                        unsigned int index )     // register
{
    unsigned int vaddr =
        SEG_MWR_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) + 
        ((channel + 1) * (MWR_CHANNEL_SPAN << 2)) +
        (index << 2);

    return ioread32( (void*)vaddr );
}

/////////////////////////////////////////////////////////////////////////////
// This function sets a new value in a channel register
// defined by the "index" and "channel") arguments, in the MWMR_DMA component
// contained in cluster "cluster_xy".
////////////////////////////////////////////////////////////////////////////yy
void _mwr_set_channel_register( unsigned int cluster_xy,  // cluster index
                                unsigned int channel,     // channel index
                                unsigned int index,       // register index
                                unsigned int value )      // written value
{
    unsigned int vaddr =
        SEG_MWR_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        ((channel + 1) * (MWR_CHANNEL_SPAN << 2)) +
        (index << 2);
        
    iowrite32( (void*)vaddr, value );
}

////////////////////////////////////////////////////////////////////////////yy
// This Interrupt service routine is called in two situations:
// - The MWR_DMA component is running in MODE_DMA_IRQ, and all the
//   communication channels have successfully completed.
// - The MWR_DMA component is running in any mode, and at least one
//   communication channel is reporting a VCI error.
////////////////////////////////////////////////////////////////////////////yy
void _mwr_isr( unsigned int irq_type,  // unused : should be HWI 
               unsigned int irq_id,    // index returned by XCU
               unsigned int bloup )    // unused : should be 0          
{
    // get coprocessor coordinates and characteristics
    // processor executing ISR and coprocessor are in the same cluster
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int cluster_id = x * Y_SIZE + y;
    unsigned int info       = _coproc_info[cluster_id];
    unsigned int nb_to      = info & 0xFF;
    unsigned int nb_from    = (info>>8) & 0xFF;

    unsigned int channel;
    unsigned int status;
    unsigned int error = 0;

    // check status, report errors and reset for all channels
    for ( channel = 0 ; channel < (nb_to + nb_from) ; channel++ )
    {
        status = _mwr_get_channel_register( cluster_xy , channel , MWR_CHANNEL_STATUS );

        if ( status == MWR_CHANNEL_BUSY )          // strange => report error
        {
            _printf("\n[GIET_ERROR] in _mwr_isr() : channel %d is busy\n", channel );
            error = 1;
        }
        else if ( status == MWR_CHANNEL_ERROR_DATA ) 
        {
            _printf("\n[GIET_ERROR] in _mwr_isr() : DATA_ERROR / channel %d\n", channel );
            error = 1;
        }
        else if ( status == MWR_CHANNEL_ERROR_LOCK )
        {
            _printf("\n[GIET_ERROR] in _mwr_isr() : LOCK_ERROR / channel %d\n", channel );
            error = 1;
        }
        else if ( status == MWR_CHANNEL_ERROR_DESC )
        {
            _printf("\n[GIET_ERROR] in _mwr_isr() : DESC_ERROR / channel %d\n", channel );
            error = 1;
        }

        // reset channel
        _mwr_set_channel_register( cluster_xy , channel , MWR_CHANNEL_RUNNING , 0 ); 
    }

    // register error
    _coproc_error[cluster_id]  = error;

    // identify task waiting on coprocessor completion
    // this task can run in a remote cluster 
    unsigned int r_gtid    = _coproc_gtid[cluster_id];
    unsigned int r_procid  = r_gtid>>16;
    unsigned int r_ltid    = r_gtid & 0xFFFF;
    unsigned int r_cluster = r_procid >> P_WIDTH;
    unsigned int r_x       = r_cluster >> Y_WIDTH;
    unsigned int r_y       = r_cluster & ((1<<Y_WIDTH)-1);
    unsigned int r_p       = r_procid & ((1<<P_WIDTH)-1);

    // Reset NORUN_MASK_IOC bit 
    static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[r_x][r_y][r_p];
    unsigned int*       ptr     = &psched->context[r_ltid][CTX_NORUN_ID];
    _atomic_and( ptr , ~NORUN_MASK_COPROC );

    // send a WAKUP WTI to processor running the sleeping task 
    _xcu_send_wti( r_cluster,   
                   r_p, 
                   0 );          // don't force context switch 

#if GIET_DEBUG_COPROC  
unsigned int p          = gpid & ((1<<P_WIDTH)-1);
_printf("\n[GIET DEBUG COPROC] P[%d,%d,%d] executes _mwr_isr() at cycle %d\n"
        "  for task %d running on P[%d,%d,%d] / error = %d\n",
        x , y , p , _get_proctime() , r_ltid , r_x , r_y , r_p , error );
#endif
}  // end _mwr_isr()



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

