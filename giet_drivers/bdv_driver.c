///////////////////////////////////////////////////////////////////////////////////
// File      : bdv_driver.c
// Date      : 23/05/2013
// Author    : alain greiner cesar fuguet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// Implementation notes:
// All accesses to BDV registers are done by the two
// _bdv_set_register() and _bdv_get_register() low-level functions,
// that are handling virtual / physical extended addressing.
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <bdv_driver.h>
#include <xcu_driver.h>
#include <mmc_driver.h>
#include <kernel_locks.h>
#include <utils.h>
#include <tty0.h>
#include <ctx_handler.h>
#include <irq_handler.h>

///////////////////////////////////////////////////////////////////////////////
//      Extern variables
///////////////////////////////////////////////////////////////////////////////

// allocated in the boot.c or kernel_init.c files
extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX]; 
 
///////////////////////////////////////////////////////////////////////////////
//      Global variables
///////////////////////////////////////////////////////////////////////////////

// lock protecting single channel BDV peripheral
__attribute__((section(".kdata")))
spin_lock_t  _bdv_lock __attribute__((aligned(64)));

// global index of the waiting task (only used in descheduling mode)
__attribute__((section(".kdata")))
unsigned int _bdv_gtid;

// BDV peripheral status (only used in descheduling mode)
__attribute__((section(".kdata")))
unsigned int _bdv_status;

///////////////////////////////////////////////////////////////////////////////
// This low_level function returns the value contained in register (index).
///////////////////////////////////////////////////////////////////////////////
unsigned int _bdv_get_register( unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_IOC_BASE + index;
    return _io_extended_read( vaddr );
}

///////////////////////////////////////////////////////////////////////////////
// This low-level function set a new value in register (index).
///////////////////////////////////////////////////////////////////////////////
void _bdv_set_register( unsigned int index,
                        unsigned int value ) 
{
    unsigned int* vaddr = (unsigned int*)SEG_IOC_BASE + index;
    _io_extended_write( vaddr, value );
}

///////////////////////////////////////////////////////////////////////////////
//      Extern functions
///////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////
unsigned int _bdv_access( unsigned int       use_irq,
                          unsigned int       to_mem,
                          unsigned int       lba,
                          unsigned long long buf_paddr,
                          unsigned int       count) 
{
    unsigned int procid  = _get_procid();
    unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH) - 1);
    unsigned int p       = procid & ((1<<P_WIDTH)-1);

#if GIET_DEBUG_IOC
if ( _get_proctime() > GIET_DEBUG_IOC )
_printf("\n[BDV DEBUG] P[%d,%d,%d] enters _bdv_access at cycle %d\n"
        "  use_irq = %d / to_mem = %d / lba = %x / paddr = %l / count = %d\n",
        x , y , p , _get_proctime() , use_irq , to_mem , lba , buf_paddr, count );
#endif

    // check buffer alignment
    if( buf_paddr & 0x3F )
    {
        _printf("\n[BDV ERROR] in _bdv_access() : buffer not cache ligne aligned\n");
        return -1;
    }

    unsigned int error;
    unsigned int status;

    // get the lock protecting BDV
    _spin_lock_acquire( &_bdv_lock );

    // set device registers
    _bdv_set_register( BLOCK_DEVICE_BUFFER    , (unsigned int)buf_paddr );
    _bdv_set_register( BLOCK_DEVICE_BUFFER_EXT, (unsigned int)(buf_paddr>>32) );
    _bdv_set_register( BLOCK_DEVICE_COUNT     , count );
    _bdv_set_register( BLOCK_DEVICE_LBA       , lba );

#if USE_IOB    // software L2/L3 cache coherence
    if ( to_mem )  _mmc_inval( buf_paddr, count<<9 );
    else           _mmc_sync( buf_paddr, count<<9 );
#endif     // end software L2/L3 cache coherence

    /////////////////////////////////////////////////////////////////////
    // In synchronous mode, we launch transfer, 
    // and poll the BDV_STATUS register until completion.
    /////////////////////////////////////////////////////////////////////
    if ( use_irq == 0 ) 
    {
        // Launch transfert
        if (to_mem == 0) _bdv_set_register( BLOCK_DEVICE_OP, BLOCK_DEVICE_WRITE );
        else             _bdv_set_register( BLOCK_DEVICE_OP, BLOCK_DEVICE_READ );

#if GIET_DEBUG_IOC
if ( _get_proctime() > GIET_DEBUG_IOC )
_printf("\n[BDV DEBUG] _bdv_access() : P[%d,%d,%d] launch transfer"
        " in polling mode at cycle %d\n",
        x , y , p , _get_proctime() );
#endif

        do
        {
            status = _bdv_get_register( BLOCK_DEVICE_STATUS );

#if GIET_DEBUG_IOC
if ( _get_proctime() > GIET_DEBUG_IOC )
_printf("\n[BDV DEBUG] _bdv_access() : P[%d,%d,%d] wait on BDV_STATUS ...\n",
        x , y , p );
#endif
        }
        while( (status != BLOCK_DEVICE_READ_SUCCESS)  &&
               (status != BLOCK_DEVICE_READ_ERROR)    &&
               (status != BLOCK_DEVICE_WRITE_SUCCESS) &&
               (status != BLOCK_DEVICE_WRITE_ERROR)   );      // busy waiting

        // analyse status
        error = ( (status == BLOCK_DEVICE_READ_ERROR) ||
                  (status == BLOCK_DEVICE_WRITE_ERROR) );
    }

    /////////////////////////////////////////////////////////////////
    // in descheduling mode, we deschedule the task
    // and use an interrupt to reschedule the task.
    // We need a critical section, because we must reset the RUN bit
	// before to launch the transfer, and we don't want to be 
    // descheduled between these two operations. 
    /////////////////////////////////////////////////////////////////
    else
    {
        unsigned int save_sr;
        unsigned int ltid = _get_current_task_id();

        // activates BDV interrupt
        _bdv_set_register( BLOCK_DEVICE_IRQ_ENABLE, 1 );

        // set _bdv_gtid 
        _bdv_gtid = (procid<<16) + ltid;

        // enters critical section
        _it_disable( &save_sr ); 

        // Set NORUN_MASK_IOC bit 
        static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
        _atomic_or( &psched->context[ltid][CTX_NORUN_ID] , NORUN_MASK_IOC );
        
        // launch transfer
        if (to_mem == 0) _bdv_set_register( BLOCK_DEVICE_OP, BLOCK_DEVICE_WRITE );
        else             _bdv_set_register( BLOCK_DEVICE_OP, BLOCK_DEVICE_READ  );

#if GIET_DEBUG_IOC
if ( _get_proctime() > GIET_DEBUG_IOC )
_printf("\n[BDV DEBUG] _bdv_access() : P[%d,%d,%d] launch transfer"
        " in descheduling mode at cycle %d\n",
        x , y , p , _get_proctime() );
#endif

        // deschedule task
        _ctx_switch();                      

#if GIET_DEBUG_IOC
if ( _get_proctime() > GIET_DEBUG_IOC )
_printf("\n[BDV DEBUG] _bdv_access() : P[%d,%d,%d] resume execution at cycle %d\n",
        x , y , p , _get_proctime() );
#endif

        // restore SR
        _it_restore( &save_sr );

        // analyse status
        error = ( (_bdv_status == BLOCK_DEVICE_READ_ERROR) ||
                  (_bdv_status == BLOCK_DEVICE_WRITE_ERROR) );
    }

    // release lock
    _spin_lock_release( &_bdv_lock );      

#if GIET_DEBUG_IOC
if ( _get_proctime() > GIET_DEBUG_IOC )
_printf("\n[BDV DEBUG] _bdv_access() : P[%d,%d,%d] exit at cycle %d\n",
        x , y , p , _get_proctime() );
#endif

    return error;
} // end _bdv_access()

////////////////////////
unsigned int _bdv_init()
{
    if ( _bdv_get_register( BLOCK_DEVICE_BLOCK_SIZE ) != 512 )
    {
        _puts("\n[GIET ERROR] in _bdv_init() : block size must be 512 bytes\n");
        return 1; 
    }

    _bdv_set_register( BLOCK_DEVICE_IRQ_ENABLE, 0 );
    return 0;
}

/////////////////////////////////////
void _bdv_isr( unsigned int irq_type,   // HWI / WTI
               unsigned int irq_id,     // index returned by ICU
               unsigned int channel )   // unused 
{
    // get BDV status and reset BDV_IRQ
    unsigned int status =  _bdv_get_register( BLOCK_DEVICE_STATUS ); 

    // check status: does nothing if IDLE or BUSY
    if ( (status == BLOCK_DEVICE_IDLE) ||
         (status == BLOCK_DEVICE_BUSY) )   return;
 
    // register status in global variable
    _bdv_status = status;

    // identify task waiting on BDV
    unsigned int procid  = _bdv_gtid>>16;
    unsigned int ltid    = _bdv_gtid & 0xFFFF;
    unsigned int cluster = procid >> P_WIDTH;
    unsigned int x       = cluster >> Y_WIDTH;
    unsigned int y       = cluster & ((1<<Y_WIDTH)-1);
    unsigned int p       = procid & ((1<<P_WIDTH)-1);

    // Reset NORUN_MASK_IOC bit 
    static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
    unsigned int*       ptr     = &psched->context[ltid][CTX_NORUN_ID];
    _atomic_and( ptr , ~NORUN_MASK_IOC );

    // send a WAKUP WTI to processor running the sleeping task 
    _xcu_send_wti( cluster,   
                   p, 
                   0 );          // don't force context switch 

#if GIET_DEBUG_IOC  
unsigned int pid  = _get_procid();
unsigned int c_x  = pid >> (Y_WIDTH + P_WIDTH);
unsigned int c_y  = (pid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int c_p  = pid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_IOC )
_printf("\n[BDV DEBUG] Processor[%d,%d,%d] enters _bdv_isr() at cycle %d\n"
        "  for task %d running on P[%d,%d,%d] / bdv_status = %x\n",
        c_x , c_y , c_p , _get_proctime() ,
        ltid , x , y , p , status );
#endif

} // end bdv_isr()


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

