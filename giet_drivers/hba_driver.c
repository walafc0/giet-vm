//////////////////////////////////////////////////////////////////////////////////
// File     : hba_driver.c
// Date     : 23/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// Implementation notes:
// All accesses to HBA registers are done by the two
// _hba_set_register() and _hba_get_register() low-level functions,
// that are handling virtual / physical extended addressing.
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <hba_driver.h>
#include <xcu_driver.h>
#include <mmc_driver.h>
#include <kernel_locks.h>
#include <utils.h>
#include <tty0.h>
#include <ctx_handler.h>
#include <irq_handler.h>
#include <vmem.h>

//////////////////////////////////////////////////////////////////////////////////
//      Extern variables
//////////////////////////////////////////////////////////////////////////////////

// allocated in the boot.c or kernel_init.c files
extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX]; 

//////////////////////////////////////////////////////////////////////////////////
//               Global variables
//////////////////////////////////////////////////////////////////////////////////
// The global variable hba_boot_mode defines the way the HBA component is used
// and must be defined in both kernel_init.c and boot.c files.
// - during the boot phase, only one processor access the HBA in synchronous
//   mode. There is no need for the allocator to use a lock.
// - after the boot phase, the HBA device can be used by several processors. The
//   allocator is protected by a sqt_lock.
//////////////////////////////////////////////////////////////////////////////////
	
extern unsigned int _hba_boot_mode;

__attribute__((section(".kdata")))
sqt_lock_t          _hba_allocator_lock  __attribute__((aligned(64)));

// state of each slot (allocated to a task or not)
// access must be protected by the allocator_lock in descheduling mode
__attribute__((section(".kdata")))
unsigned int        _hba_allocated_cmd[32];

// state of the command (active or not), for each possible slot
// used only in descheduling mode
__attribute__((section(".kdata")))
unsigned int        _hba_active_cmd[32]; 

// global index of the task, for each entry in the command list
__attribute__((section(".kdata")))
unsigned int        _hba_gtid[32];

// status of HBA commands
__attribute__((section(".kdata")))
unsigned int        _hba_status;

// command list : up to 32 commands
__attribute__((section(".kdata")))
hba_cmd_desc_t      _hba_cmd_list[32] __attribute__((aligned(0x40)));   

// command tables array : one command table per entry in command list
__attribute__((section(".kdata")))
hba_cmd_table_t     _hba_cmd_table[32] __attribute__((aligned(0x40))); 


//////////////////////////////////////////////////////////////////////////////
// This low level function returns the value of register (index)
//////////////////////////////////////////////////////////////////////////////
unsigned int _hba_get_register( unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_IOC_BASE + index;
    return _io_extended_read( vaddr );
}

//////////////////////////////////////////////////////////////////////////////
// This low level function set a new value in register (index)  
//////////////////////////////////////////////////////////////////////////////
void _hba_set_register( unsigned int index,
                        unsigned int value )
{
    unsigned int* vaddr = (unsigned int*)SEG_IOC_BASE + index;
    _io_extended_write( vaddr, value );
}

///////////////////////////////////////////////////////////////////////////////
//      Extern functions
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// This blocking fonction allocates a free command index to the task. 
// The hba_allocator_lock is not used in boot mode.
// It returns the allocated command index (between 0 and 31)
///////////////////////////////////////////////////////////////////////////////
unsigned int _hba_cmd_alloc()
{
    unsigned int found = 0;
    unsigned int c;           // command index for the loop
    unsigned int cmd_id = -1; // allocated command index when found

    while ( found == 0)
    {
        if ( !_hba_boot_mode )
            _sqt_lock_acquire(&_hba_allocator_lock);

        for ( c = 0; c < 32 ; c++ )
        {
            if (_hba_allocated_cmd[c] == 0)
            {
                found = 1;
                cmd_id = c;
                _hba_allocated_cmd[c] = 1;
                break;
            }
        }

        if ( !_hba_boot_mode )
            _sqt_lock_release(&_hba_allocator_lock);
    }

    return cmd_id;
}

///////////////////////////////////////////////////////////////////////////////
// This function releases the command index in the hba_allocated_cmd table.
// There is no need to take the lock because only the task which owns the 
// command can release it.
// return 0 if success, -1 if error
///////////////////////////////////////////////////////////////////////////////
unsigned int _hba_cmd_release(unsigned int cmd_id)
{
    if ( _hba_allocated_cmd[cmd_id] == 0 )
    {
        _printf("\n[HBA ERROR] in _hba_access() : ask to release a command which is not allocated\n");
        return -1;
    }
    
    _hba_allocated_cmd[cmd_id] = 0;
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// This function gets a command index with the hba_cmd_alloc function. Then it
// registers a command in both the command list and the command table. It
// updates the HBA_PXCI register and the hba_active_cmd in descheduling mode.
// At the end the command slot is released.
// return 0 if success, -1 if error
///////////////////////////////////////////////////////////////////////////////
unsigned int _hba_access( unsigned int       use_irq,
                          unsigned int       to_mem,
                          unsigned int       lba,  
                          unsigned long long buf_paddr,
                          unsigned int       count )   
{
    unsigned int procid  = _get_procid();
    unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH) - 1);
    unsigned int p       = procid & ((1<<P_WIDTH)-1);

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG HBA] _hba_access() : P[%d,%d,%d] enters at cycle %d\n"
        "  use_irq = %d / to_mem = %d / lba = %x / paddr = %l / count = %d\n",
        x , y , p , _get_proctime() , use_irq , to_mem , lba , buf_paddr, count );
#endif

    unsigned int       cmd_id;            // command index
    unsigned int       pxci;              // HBA_PXCI register value
    unsigned int       pxis;              // HBA_PXIS register value
    hba_cmd_desc_t*    cmd_desc;          // command descriptor pointer   
    hba_cmd_table_t*   cmd_table;         // command table pointer

    // check buffer alignment
    if( buf_paddr & 0x3F )
    {
        _printf("\n[HBA ERROR] in _hba_access() : buffer not 64 bytes aligned\n");
        return -1;
    }

    // get one entry in Command List
    cmd_id = _hba_cmd_alloc();

    // compute pointers on command descriptor and command table    
    cmd_desc  = &_hba_cmd_list[cmd_id];
    cmd_table = &_hba_cmd_table[cmd_id];

    // set  buffer descriptor in command table 
    cmd_table->buffer.dba  = (unsigned int)(buf_paddr);
    cmd_table->buffer.dbau = (unsigned int)(buf_paddr >> 32);
    cmd_table->buffer.dbc  = count * 512;

    // initialize command table header
    cmd_table->header.lba0 = (char)lba;
    cmd_table->header.lba1 = (char)(lba>>8);
    cmd_table->header.lba2 = (char)(lba>>16);
    cmd_table->header.lba3 = (char)(lba>>24);
    cmd_table->header.lba4 = 0;
    cmd_table->header.lba5 = 0;

    // initialise command descriptor
    cmd_desc->prdtl[0] = 1;
    cmd_desc->prdtl[1] = 0;
    if( to_mem ) cmd_desc->flag[0] = 0x00;
    else         cmd_desc->flag[0] = 0x40;     

#if USE_IOB    // software L2/L3 cache coherence

    // compute physical addresses
    unsigned long long cmd_desc_paddr;    // command descriptor physical address
    unsigned long long cmd_table_paddr;   // command table header physical address
    unsigned int       flags;             // unused

    if ( _get_mmu_mode() & 0x4 )
    {
        cmd_desc_paddr  = _v2p_translate( (unsigned int)cmd_desc  , &flags );
        cmd_table_paddr = _v2p_translate( (unsigned int)cmd_table , &flags );
    }
    else
    {
        cmd_desc_paddr  = (unsigned int)cmd_desc;
        cmd_table_paddr = (unsigned int)cmd_table;
    }

    // update external memory for command table 
    _mmc_sync( cmd_table_paddr & (~0x3F) , sizeof(hba_cmd_table_t) );

    // update external memory for command descriptor
    _mmc_sync( cmd_desc_paddr & (~0x3F) , sizeof(hba_cmd_desc_t) );

    // inval or synchronize memory buffer
    if ( to_mem )  _mmc_inval( buf_paddr, count<<9 );
    else           _mmc_sync( buf_paddr, count<<9 );

#endif     // end software L2/L3 cache coherence

    /////////////////////////////////////////////////////////////////////
    // In synchronous mode, we poll the PXCI register until completion
    /////////////////////////////////////////////////////////////////////
    if ( use_irq == 0 ) 
    {
        // start HBA transfer
        _hba_set_register( HBA_PXCI, (1<<cmd_id) );

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG HBA] _hba_access() : P[%d,%d,%d] get slot %d in Cmd List "
        " at cycle %d / polling\n",
        x , y , p , cmd_id, _get_proctime() );
#endif
        // disable IRQs in PXIE register
        _hba_set_register( HBA_PXIE , 0 );

        // poll PXCI[cmd_id] until command completed by HBA
        do
        {
            pxci = _hba_get_register( HBA_PXCI );

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG HBA] _hba_access() : P[%d,%d,%d] wait on HBA_PXCI / pxci = %x\n",
        x , y , p , pxci );
#endif
        }
        while( pxci & (1<<cmd_id) ); 
             
        // get PXIS register
        pxis = _hba_get_register( HBA_PXIS );

        // reset PXIS register
        _hba_set_register( HBA_PXIS , 0 );
    }

    /////////////////////////////////////////////////////////////////
    // in descheduling mode, we deschedule the task
    // and use an interrupt to reschedule the task.
    // We need a critical section, because we must set the NORUN bit
	// before to launch the transfer, and we don't want to be 
    // descheduled between these two operations. 
    /////////////////////////////////////////////////////////////////
    else
    {

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG HBA] _hba_access() : P[%d,%d,%d] get slot %d in Cmd List "
        "at cycle %d / descheduling\n",
        x , y , p , cmd_id, _get_proctime() );
#endif
        unsigned int save_sr;
        unsigned int ltid = _get_current_task_id();

        // activates HBA interrupts 
        _hba_set_register( HBA_PXIE , 0x00000001 ); 

        // set _hba_gtid[cmd_id] 
        _hba_gtid[cmd_id] = (procid<<16) + ltid;

        // enters critical section
        _it_disable( &save_sr ); 

        // Set NORUN_MASK_IOC bit 
        static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
        unsigned int*       ptr     = &psched->context[ltid][CTX_NORUN_ID];
        _atomic_or( ptr , NORUN_MASK_IOC );
      
        // start HBA transfer
        _hba_set_register( HBA_PXCI, (1<<cmd_id) );

        // set _hba_active_cmd[cmd_id]
        _hba_active_cmd[cmd_id] = 1;

        // deschedule task
        _ctx_switch();                      

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG HBA] _hba_access() : task %d on P[%d,%d,%d] resume at cycle %d\n",
        ltid , x , y , p , _get_proctime() );
#endif

        // restore SR
        _it_restore( &save_sr );

        // get command status
        pxis = _hba_status;
    }
    
    // release the cmd index
    unsigned int release_success;
    release_success = _hba_cmd_release(cmd_id);

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG HBA] _hba_access() : P[%d,%d,%d] release slot %d in Cmd List "
        "and exit at cycle %d\n",
        x , y , p, cmd_id,
        _get_proctime() );
#endif

    if ( release_success != 0 )   return -1;
    else if ( pxis & 0x40000000 ) return pxis;
    else                          return 0;

} // end _hba_access()


////////////////////////
unsigned int _hba_init()
{
    unsigned int       cmd_list_vaddr;
    unsigned int       cmd_table_vaddr;
    unsigned long long cmd_list_paddr;
    unsigned long long cmd_table_paddr;
    unsigned int       flags;            // unused

    // compute Command list & command table physical addresses
    cmd_list_vaddr  = (unsigned int)(&_hba_cmd_list[0]);
    cmd_table_vaddr = (unsigned int)(&_hba_cmd_table[0]);
    if ( _get_mmu_mode() & 0x4 )
    {
        cmd_list_paddr  = _v2p_translate( cmd_list_vaddr  , &flags );
        cmd_table_paddr = _v2p_translate( cmd_table_vaddr , &flags );
    }
    else
    {
        cmd_list_paddr  = (unsigned long long)cmd_list_vaddr;
        cmd_table_paddr = (unsigned long long)cmd_table_vaddr;
    }

    // initialise allocator lock if not in boot mode
    if ( !_hba_boot_mode )
        _sqt_lock_init(&_hba_allocator_lock);

    // initialise Command Descriptors in Command List, allocated command table
    // and active command table
    unsigned int         c;      
    unsigned long long   paddr;
    for( c=0 ; c<32 ; c++ )
    {
        paddr = cmd_table_paddr + c * sizeof(hba_cmd_table_t);
        _hba_cmd_list[c].ctba  = (unsigned int)(paddr);
        _hba_cmd_list[c].ctbau = (unsigned int)(paddr>>32);
        _hba_allocated_cmd[c] = 0;
        _hba_active_cmd[c] = 0;
    }

    // initialise HBA registers 
    _hba_set_register( HBA_PXCLB  , (unsigned int)(cmd_list_paddr) );
    _hba_set_register( HBA_PXCLBU , (unsigned int)(cmd_list_paddr>>32) );
    _hba_set_register( HBA_PXIE   , 0 );
    _hba_set_register( HBA_PXIS   , 0 );
    _hba_set_register( HBA_PXCI   , 0 );
    _hba_set_register( HBA_PXCMD  , 1 );

    return 0;
}


/////////////////////////////////////////////////////
void _hba_isr( unsigned int irq_type,   // HWI / WTI
               unsigned int irq_id,     // index returned by ICU
               unsigned int channel )   // unused 
{
    // save PXIS register if there is no previous error
    if ( !(_hba_status & 0x40000000))
        _hba_status = _hba_get_register( HBA_PXIS );

    // reset PXIS register
    _hba_set_register( HBA_PXIS , 0 );

    unsigned int cmd_id;  // cmd index for the loops
    
    // save the current list of active cmd in a 32 bits word
    unsigned int current_active_cmd = 0;
    for ( cmd_id = 0 ; cmd_id < 32 ; cmd_id ++ )
    {
        if ( _hba_active_cmd[cmd_id] == 1 ) current_active_cmd += (1 << cmd_id);
    }
    
    // get HBA_PXCI containing commands status
    unsigned int current_pxci = _hba_get_register( HBA_PXCI );

    for ( cmd_id = 0 ; cmd_id < 32 ; cmd_id ++ )
    {
        if ( ( (current_active_cmd & (1<<cmd_id)) != 0) && // active command
             ( (current_pxci & (1<<cmd_id)) == 0 ) )       // completed command
        {
            // desactivate the command
            _hba_active_cmd[cmd_id] = 0;

            // identify waiting task 
            unsigned int procid  = _hba_gtid[cmd_id]>>16;
            unsigned int ltid    = _hba_gtid[cmd_id] & 0xFFFF;
            unsigned int cluster = procid >> P_WIDTH;
            unsigned int x       = cluster >> Y_WIDTH;
            unsigned int y       = cluster & ((1<<Y_WIDTH)-1);
            unsigned int p       = procid & ((1<<P_WIDTH)-1);
 
            // Reset NORUN_MASK_IOC bit 
            static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
            _atomic_and( &psched->context[ltid][CTX_NORUN_ID] , ~NORUN_MASK_IOC );

            // send a WAKUP WTI to processor running the waiting task 
            _xcu_send_wti( cluster , 
                           p , 
                           0 );          // don't force context switch

#if GIET_DEBUG_IOC  
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG HBA] _hba_isr() : command %d completed at cycle %d\n"
        "  resume task %d running on P[%d,%d,%d]\n",
        cmd_id , _get_proctime() ,
        ltid , x , y , p );
#endif 
        }
    }
} // end _hba_isr()

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

