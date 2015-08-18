////////////////////////////////////////////////////////////////////////////////////
// File     : kernel_init.c
// Date     : 26/05/2012
// Authors  : alain greiner & mohamed karaoui
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////////
// This kernel_init.c file is part of the GIET-VM nano-kernel.
////////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <utils.h>
#include <vmem.h>
#include <tty0.h>
#include <kernel_malloc.h>
#include <kernel_locks.h>
#include <kernel_barriers.h>
#include <fat32.h>
#include <xcu_driver.h>
#include <nic_driver.h>
#include <hba_driver.h>
#include <sdc_driver.h>
#include <bdv_driver.h>
#include <mmc_driver.h>
#include <ctx_handler.h>
#include <irq_handler.h>
#include <mapping_info.h>
#include <mips32_registers.h>

#if !defined(X_SIZE) 
# error: You must define X_SIZE in the hard_config.h file
#endif

#if !defined(Y_SIZE) 
# error: You must define Y_SIZE in the hard_config.h file
#endif

#if !defined(Y_WIDTH) 
# error: You must define Y_WIDTH in the hard_config.h file
#endif

#if !defined(Y_WIDTH) 
# error: You must define Y_WIDTH in the hard_config.h file
#endif

#if !defined(NB_PROCS_MAX)
# error: You must define NB_PROCS_MAX in the hard_config.h file
#endif

#if !defined(NB_TOTAL_PROCS)
# error: You must define NB_TOTAL_PROCS in the hard_config.h file
#endif

#if !defined(USE_XCU) 
# error: You must define USE_XCU in the hard_config.h file
#endif

#if !defined(USE_PIC) 
# error: You must define USE_PIC in the hard_config.h file
#endif

#if !defined(IDLE_TASK_INDEX) 
# error: You must define IDLE_TASK_INDEX in the ctx_handler.h file
#endif

#if !defined(GIET_TICK_VALUE) 
# error: You must define GIET_TICK_VALUE in the giet_config.h file
#endif

#if !defined(GIET_NB_VSPACE_MAX)
# error: You must define GIET_NB_VSPACE_MAX in the giet_config.h file
#endif

#if !defined(NB_TTY_CHANNELS)
# error: You must define NB_TTY_CHANNELS in the hard_config.h file
#endif

#if (NB_TTY_CHANNELS < 1)
# error: NB_TTY_CHANNELS cannot be smaller than 1
#endif

#if !defined(GIET_ISR_TYPE_MAX)
# error: You must define GIET_ISR_TYPE_MAX in the giet_config.h file
#endif

#if !defined(GIET_ISR_CHANNEL_MAX)
# error: You must define GIET_ISR_CHANNEL_MAX in the giet_config.h file
#endif


////////////////////////////////////////////////////////////////////////////////
//       Global variables
////////////////////////////////////////////////////////////////////////////////

// array of page tables virtual addresses
__attribute__((section(".kdata")))
volatile unsigned int _ptabs_vaddr[GIET_NB_VSPACE_MAX][X_SIZE][Y_SIZE]; 

// array of page tables PTPR values (physical addresses >> 13)
__attribute__((section(".kdata")))
volatile unsigned int _ptabs_ptprs[GIET_NB_VSPACE_MAX][X_SIZE][Y_SIZE]; 

// Array of pointers on the schedulers
__attribute__((section(".kdata")))
volatile static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX]; 

// Synchonisation before entering parallel execution
__attribute__((section(".kdata")))
volatile unsigned int _kernel_init_done = 0;

// Kernel uses sqt_lock to protect TTY0        
__attribute__((section(".kdata")))
unsigned int   _tty0_boot_mode = 0;

// Kernel uses sqt_lock to protect command allocator in HBA        
__attribute__((section(".kdata")))
unsigned int   _hba_boot_mode = 0;

// synchronisation barrier for parallel init by all processors      
__attribute__((section(".kdata")))
sqt_barrier_t  _all_procs_barrier  __attribute__((aligned(64)));

////////////////////////////////////////////////////////////////////////////////
//      Extern variables
////////////////////////////////////////////////////////////////////////////////

// this variable is defined in tty0.c file
extern sqt_lock_t _tty0_sqt_lock;

// this variable is allocated in mmc_kernel.c
extern unsigned int _mmc_boot_mode;

////////////////////////////////////////////////////////////////////////////////
// This kernel_init() function completes the kernel initialisation in 6 steps:
// Step 0 is done by processor[0,0,0]. Steps 1 to 4 are executed in parallel
// by all processors.
// - step 0 : P[0,0,0] Initialise various global variables.
// - step 1 : Each processor initialises scheduler pointers array.
// - step 2 : Each processor initialises PTAB pointers arrays.
// - step 3 : Each processor initialise idle task and starts TICK timer.
// - step 4 : Each processor set sp, sr, ptpr, epc registers values. 
////////////////////////////////////////////////////////////////////////////////
__attribute__((section (".kinit"))) void kernel_init() 
{
    // gpid  : hardware processor index (fixed format: X_WIDTH|Y_WIDTH|P_WIDTH)
    // x,y,p : proc coordinates ( x < X_SIZE / y < Y_SIZE / p < NB_PROCS_MAX )

    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH & ((1<<X_WIDTH)-1);
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p          = gpid & ((1<<P_WIDTH)-1);
    unsigned int unused;
    
    ////////////////////////////////////////////////////////////////////////////
    // Step 0 : P[0,0,0] initialises global variables and peripherals
   ////////////////////////////////////////////////////////////////////////////

    if ( gpid == 0 )
    {
        //////  distributed kernel heap initialisation
        _heap_init();
        
#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes kernel heap init\n", x, y, p );
#endif
        //////  distributed lock for MMC
        _mmc_boot_mode = 0;
        _mmc_init_locks();

#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes MMC distributed lock init\n", x , y , p );
#endif
        //////  distributed lock for TTY0
        _sqt_lock_init( &_tty0_sqt_lock );

#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes TTY0 lock init\n", x , y , p );
#endif
        //////  distributed kernel barrier between all processors
        _sqt_barrier_init( &_all_procs_barrier );

#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes barrier init\n", x , y , p );
#endif

        ////// _ext_irq_index[isr][channel] initialisation
        if ( USE_PIC ) _ext_irq_init();

#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes ext_irq init\n", x , y , p );
#endif

        //////  NIC peripheral initialization
        if ( USE_NIC ) _nic_global_init( 1,      // broadcast accepted
                                         1,      // bypass activated
                                         0,      // tdm non activated
                                         0 );    // tdm period
#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes NIC init\n", x , y , p );
#endif

        //////  IOC peripheral initialisation
        if ( USE_IOC_HBA )
        {
            _hba_init();
            _ext_irq_alloc( ISR_HBA , 0 , &unused );

#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes HBA init\n", x , y , p );
#endif
        }
        if ( USE_IOC_SDC )
        {
            _sdc_init();
            _ext_irq_alloc( ISR_SDC , 0 , &unused );

#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes SDC init\n", x , y , p );
#endif
        }
        if ( USE_IOC_BDV )
        {
            _bdv_init();
            _ext_irq_alloc( ISR_BDV , 0 , &unused );

#if GIET_DEBUG_INIT
_nolock_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes BDV init\n", x , y , p );
#endif
        }

        //////  release other processors
        _kernel_init_done = 1;
    }
    else 
    {
        while( _kernel_init_done == 0 )  asm volatile ( "nop" );
    }

    ///////////////////////////////////////////////////////////////////////////
    // Step 1 : each processor get its scheduler vaddr from CP0_SCHED, 
    //          contributes to _schedulers[] array initialisation,
    //          and wait completion of array initialisation.
    ///////////////////////////////////////////////////////////////////////////

    static_scheduler_t* psched     = (static_scheduler_t*)_get_sched();
    unsigned int        tasks      = psched->tasks;

    _schedulers[x][y][p] = psched;

#if GIET_DEBUG_INIT
_printf("\n[DEBUG KINIT] P[%d,%d,%d] initialises SCHED array\n"
        " - scheduler vbase = %x\n"
        " - tasks           = %d\n",
        x, y, p, (unsigned int)psched, tasks );
#endif

    /////////////////////////////////////////
    _sqt_barrier_wait( &_all_procs_barrier );    
    /////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // step 2 : each processor that is allocated at least one task loops 
    //          on its allocated tasks: 
    //          - contributes to _ptabs_vaddr[][][] & _ptabs_ptprs[][][]
    //            initialisation, from values stored in the tasks contexts.
    //          - set CTX_RA slot  with the kernel _ctx_eret() virtual address.
    //          - set CTX_ENTRY slot that must contain the task entry point, 
    //            and contain only at this point the virtual address of the 
    //            memory word containing this entry point. 
    ////////////////////////////////////////////////////////////////////////////

    unsigned int ltid;

    for (ltid = 0; ltid < tasks; ltid++) 
    {
        unsigned int vsid = _get_task_slot( x, y, p, ltid , CTX_VSID_ID ); 
        unsigned int ptab = _get_task_slot( x, y, p, ltid , CTX_PTAB_ID ); 
        unsigned int ptpr = _get_task_slot( x, y, p, ltid , CTX_PTPR_ID ); 

        // initialize PTABS arrays
        _ptabs_vaddr[vsid][x][y] = ptab;
        _ptabs_ptprs[vsid][x][y] = ptpr;

        // set the PTPR to use the local page table
        asm volatile( "mtc2    %0,   $0"
                      : : "r" (ptpr) );

        // set CTX_RA slot
        unsigned int ctx_ra = (unsigned int)(&_ctx_eret);
        _set_task_slot( x, y, p, ltid, CTX_RA_ID, ctx_ra );

        // set CTX_ENTRY slot
        unsigned int* ptr = (unsigned int*)_get_task_slot(x , y , p , ltid , CTX_ENTRY_ID);
        unsigned int ctx_entry = *ptr;
        _set_task_slot( x , y , p , ltid , CTX_ENTRY_ID , ctx_entry );

#if GIET_DEBUG_INIT
_printf("\n[DEBUG KINIT] P[%d,%d,%d] initialises PTABS arrays"
        " and context for task %d \n"
        " - ptabs_vaddr[%d][%d][%d] = %x\n"
        " - ptabs_paddr[%d][%d][%d] = %l\n"
        " - ctx_entry            = %x\n"
        " - ctx_ra               = %x\n",
        x , y , p , ltid ,  
        vsid , x , y , ptab ,
        vsid , x , y , ((unsigned long long)ptpr)<<13 ,
        ctx_entry, ctx_ra );
#endif

    }  // end for tasks

    /////////////////////////////////////////
    _sqt_barrier_wait( &_all_procs_barrier );    
    /////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // step 3 : - Each processor complete idle task context initialisation.
    //            Only CTX_SP, CTX_RA, CTX_EPC, CTX_ENTRY slots, because other
    //            slots have been initialised in boot code)
    //            The 4 Kbytes idle stack is implemented in the scheduler itself.
    //          - Each processor starts TICK timer, if at least one task.
    //          - P[0,0,0] initialises FAT (not done before, because it must 
    //            be done after the _ptabs_vaddr[v][x][y] array initialisation, 
    //            for V2P translation in _fat_ioc_access() function).
    ////////////////////////////////////////////////////////////////////////////

    unsigned int sp    = ((unsigned int)psched) + 0x2000;
    unsigned int ra    = (unsigned int)(&_ctx_eret);
    unsigned int entry = (unsigned int)(&_idle_task);

    _set_task_slot( x , y , p , IDLE_TASK_INDEX , CTX_SP_ID  , sp    );
    _set_task_slot( x , y , p , IDLE_TASK_INDEX , CTX_RA_ID  , ra    );
    _set_task_slot( x , y , p , IDLE_TASK_INDEX , CTX_EPC_ID , entry );
    _set_task_slot( x , y , p , IDLE_TASK_INDEX , CTX_ENTRY_ID , entry );

    if (tasks > 0) _xcu_timer_start( cluster_xy, p, GIET_TICK_VALUE ); 

#if GIET_DEBUG_INIT
_printf("\n[DEBUG KINIT] P[%d,%d,%d] initializes idle_task and starts TICK\n",  
        x, y, p );
#endif

    if ( gpid == 0 )
    {
        _fat_init( 1 );   // kernel mode => Inode-Tree, Fat-Cache and File-Caches

#if GIET_DEBUG_INIT
_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes kernel FAT init\n",
        x, y, p );
#endif

    }

    /////////////////////////////////////////
    _sqt_barrier_wait( &_all_procs_barrier );    
    /////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // step 4 : Each processor computes the task index (ltid), and the values
    //          to initialize the SP, SR, PTPR, EPC registers.
    //          It jumps to a runable task if possible, and jumps to IDLE-TASK 
    //          if no task allocated or no runable task.
    ////////////////////////////////////////////////////////////////////////////

    if (tasks == 0) _printf("\n[GIET WARNING] No task allocated to P[%d,%d,%d]\n",
                            x, y, p );

    // default value for ltid
    ltid = IDLE_TASK_INDEX;

    // scan allocated tasks to find a runable task
    unsigned int  task_id; 
    for ( task_id = 0 ; task_id < tasks ; task_id++ )
    {
        if ( _get_task_slot( x, y, p, task_id, CTX_NORUN_ID ) == 0 )
        {
            ltid = task_id;
            break;
        }
    }

    // update scheduler
    psched->current = ltid;

    // get values from selected task context
    unsigned int sp_value   = _get_task_slot( x, y, p, ltid, CTX_SP_ID);
    unsigned int sr_value   = _get_task_slot( x, y, p, ltid, CTX_SR_ID);
    unsigned int ptpr_value = _get_task_slot( x, y, p, ltid, CTX_PTPR_ID);
    unsigned int epc_value  = _get_task_slot( x, y, p, ltid, CTX_ENTRY_ID);

#if GIET_DEBUG_INIT
_printf("\n[DEBUG KINIT] P[%d,%d,%d] completes kernel_init at cycle %d\n"
        " ltid = %d / sp = %x / sr = %x / ptpr = %x / epc = %x\n",
        x , y , p , _get_proctime() ,
        ltid , sp_value , sr_value , ptpr_value , epc_value );
#endif

    // set registers and jump to user code
    asm volatile ( "move  $29,  %0                  \n"   /* SP <= ctx[CTX_SP_ID] */
                   "mtc0  %1,   $12                 \n"   /* SR <= ctx[CTX_SR_ID] */
                   "mtc2  %2,   $0                  \n"   /* PTPR <= ctx[CTX_PTPR] */
                   "mtc0  %3,   $14                 \n"   /* EPC <= ctx[CTX_EPC]  */
                   "eret                            \n"   /* jump to user code  */
                   "nop                             \n"
                   : 
                   : "r"(sp_value), "r"(sr_value), "r"(ptpr_value), "r"(epc_value)
                   : "$29", "memory" );

} // end kernel_init()


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

