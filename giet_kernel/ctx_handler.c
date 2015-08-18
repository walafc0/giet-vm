//////////////////////////////////////////////////////////////////////////////////
// File     : ctx_handler.c
// Date     : 01/04/2012
// Authors  : alain greiner & joel porquet
// Copyright (c) UPMC-LIP6
//////////////////////////////////////////////////////////////////////////////////

#include <ctx_handler.h>
#include <sys_handler.h>
#include <giet_config.h>
#include <hard_config.h>
#include <utils.h>
#include <tty0.h>
#include <xcu_driver.h>

/////////////////////////////////////////////////////////////////////////////////
//     Extern variables and functions
/////////////////////////////////////////////////////////////////////////////////

// defined in giet_kernel/switch.s file
extern void _task_switch(unsigned int *, unsigned int *);

// allocated in boot.c or kernel_init.c files
extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX];

//////////////////
static void _ctx_kill_task( unsigned int ltid )
{
    // get scheduler address
    static_scheduler_t* psched = (static_scheduler_t*)_get_sched();

    // pretend the task to kill is scheduled (required for sys_handler calls)
    unsigned int cur_task = psched->current;
    psched->current = ltid;

    // release private TTY terminal if required
    if ( psched->context[ltid][CTX_TTY_ID] < NB_TTY_CHANNELS )
    {
        _sys_tty_release();
        psched->context[ltid][CTX_TTY_ID] = -1;
    }

    // release private TIM channel if required
    if ( psched->context[ltid][CTX_TIM_ID] < NB_TIM_CHANNELS )
    {
        _sys_tim_release();
        psched->context[ltid][CTX_TIM_ID] = -1;
    }

    // release private NIC_RX channel if required
    if ( psched->context[ltid][CTX_NIC_RX_ID] < NB_NIC_CHANNELS )
    {
        _sys_nic_release( 1 );
        psched->context[ltid][CTX_NIC_RX_ID] = -1;
    }

    // release private NIC_TX channel if required
    if ( psched->context[ltid][CTX_NIC_TX_ID] < NB_NIC_CHANNELS )
    {
        _sys_nic_release( 0 );
        psched->context[ltid][CTX_NIC_TX_ID] = -1;
    }

    // release private FBF_CMA channel if required
    if ( psched->context[ltid][CTX_CMA_FB_ID] < NB_CMA_CHANNELS )
    {
        _sys_fbf_cma_release();
        psched->context[ltid][CTX_CMA_FB_ID] = -1;
    }

    // restore scheduled task
    psched->current = cur_task;

    // set NORUN_MASK_TASK bit
    _atomic_or( &psched->context[ltid][CTX_NORUN_ID], NORUN_MASK_TASK );
}


//////////////////
static void _ctx_exec_task( unsigned int ltid )
{
    // get scheduler address
    static_scheduler_t* psched = (static_scheduler_t*)_get_sched();

    // TODO: reload .data segment

    // find initial stack pointer
    mapping_header_t * header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_task_t   * task    = _get_task_base(header);
    mapping_vseg_t   * vseg    = _get_vseg_base(header);
    unsigned int task_id       = psched->context[ltid][CTX_GTID_ID];
    unsigned int vseg_id       = task[task_id].stack_vseg_id;
    unsigned int sp_value      = vseg[vseg_id].vbase + vseg[vseg_id].length;

    // reset task context: RA / SR / SP / EPC / NORUN
    psched->context[ltid][CTX_RA_ID]    = (unsigned int)&_ctx_eret;
    psched->context[ltid][CTX_SR_ID]    = GIET_SR_INIT_VALUE;
    psched->context[ltid][CTX_SP_ID]    = sp_value;
    psched->context[ltid][CTX_EPC_ID]   = psched->context[ltid][CTX_ENTRY_ID];
    psched->context[ltid][CTX_NORUN_ID] = 0;
}


//////////////////////////////////
void _ctx_display( unsigned int x,
                   unsigned int y,
                   unsigned int p,
                   unsigned int ltid,
                   char*        string )
{
    static_scheduler_t* psched = _schedulers[x][y][p];
    _printf("\n########## task[%d,%d,%d,%d] context\n"
            " - CTX_EPC   = %x\n"
            " - CTX_PTAB  = %x\n"
            " - CTX_PTPR  = %x\n"
            " - CTX_VSID  = %x\n"
            " - CTX_SR    = %x\n"
            " - CTX_RA    = %x\n"
            " - CTX_SP    = %x\n"
            " - CTX_NORUN = %x\n"
            " - CTX_SIG   = %x\n"
            "########## %s\n",
            x , y , p , ltid ,
            psched->context[ltid][CTX_EPC_ID], 
            psched->context[ltid][CTX_PTAB_ID], 
            psched->context[ltid][CTX_PTPR_ID], 
            psched->context[ltid][CTX_VSID_ID], 
            psched->context[ltid][CTX_SR_ID], 
            psched->context[ltid][CTX_RA_ID], 
            psched->context[ltid][CTX_SP_ID], 
            psched->context[ltid][CTX_NORUN_ID],
            psched->context[ltid][CTX_SIG_ID],
            string );
}  // _ctx_display()


//////////////////
void _ctx_switch() 
{
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int lpid       = gpid & ((1<<P_WIDTH)-1);

    // get scheduler address
    static_scheduler_t* psched = (static_scheduler_t*)_get_sched();

    // get number of tasks allocated to scheduler
    unsigned int tasks = psched->tasks;

    // get current task index
    unsigned int curr_task_id = psched->current;

    // select the next task using a round-robin policy
    unsigned int next_task_id;
    unsigned int tid;
    unsigned int found = 0;

    for (tid = curr_task_id + 1; tid < curr_task_id + 1 + tasks; tid++) 
    {
        next_task_id = tid % tasks;

        // this task needs to be killed
        if ( psched->context[next_task_id][CTX_SIG_ID] & SIG_MASK_KILL )
        {
            _ctx_kill_task( next_task_id );

            // acknowledge signal
            _atomic_and( &psched->context[next_task_id][CTX_SIG_ID], ~SIG_MASK_KILL );
        }

        // this task needs to be executed
        if ( psched->context[next_task_id][CTX_SIG_ID] & SIG_MASK_EXEC )
        {
            _ctx_exec_task( next_task_id );

            // acknowledge signal
            _atomic_and( &psched->context[next_task_id][CTX_SIG_ID], ~SIG_MASK_EXEC );
        }

        // test if the task is runable
        if ( psched->context[next_task_id][CTX_NORUN_ID] == 0 ) 
        {
            found = 1;
            // TODO: don't break to process all pending signals.
            break;
        }
    }

    // launch "idle" task if no runable task
    if (found == 0) next_task_id = IDLE_TASK_INDEX;

#if GIET_DEBUG_SWITCH
unsigned int x = cluster_xy >> Y_WIDTH;
unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
_printf("\n[DEBUG SWITCH] (%d) -> (%d) on processor[%d,%d,%d] at cycle %d\n",
        curr_task_id, next_task_id, x, y , lpid, _get_proctime() );
#endif

    if (curr_task_id != next_task_id)  // actual task switch required
    {
        unsigned int* curr_ctx_vaddr = &(psched->context[curr_task_id][0]);
        unsigned int* next_ctx_vaddr = &(psched->context[next_task_id][0]);

        // reset TICK timer counter. 
        _xcu_timer_reset_cpt( cluster_xy, lpid );

        // set current task index 
        psched->current = next_task_id;

        // makes context switch
        _task_switch( curr_ctx_vaddr , next_ctx_vaddr );
    }
} //end _ctx_switch()


/////////////////
void _idle_task() 
{
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p          = gpid & ((1<<P_WIDTH)-1);

    while(1)
    {
        // initialize counter
        unsigned int count = GIET_IDLE_TASK_PERIOD;

        // decounting loop
        asm volatile(
                "move   $3,   %0              \n"
                "_idle_task_loop:             \n"
                "addi   $3,   $3,   -1        \n"
                "bnez   $3,   _idle_task_loop \n"
                "nop                          \n"
                :
                : "r"(count)
                : "$3" ); 

        // warning message
        _printf("\n[GIET WARNING] Processor[%d,%d,%d] still idle at cycle %d",
                x , y , p , _get_proctime() );
    }
} // end ctx_idle()


////////////////
void _ctx_eret() 
{
    asm volatile("eret");
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

