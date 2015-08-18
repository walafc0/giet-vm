///////////////////////////////////////////////////////////////////////////////////
// File     : sys_handler.c
// Date     : 01/04/2012
// Author   : alain greiner and joel porquet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <sys_handler.h>
#include <tty_driver.h>
#include <tim_driver.h>
#include <nic_driver.h>
#include <mmc_driver.h>
#include <mwr_driver.h>
#include <cma_driver.h>
#include <ctx_handler.h>
#include <fat32.h>
#include <utils.h>
#include <kernel_malloc.h>
#include <tty0.h>
#include <vmem.h>
#include <hard_config.h>
#include <giet_config.h>
#include <mapping_info.h>
#include <irq_handler.h>
#include <io.h>

#if !defined(SEG_BOOT_MAPPING_BASE) 
# error: You must define SEG_BOOT_MAPPING_BASE in the hard_config.h file
#endif

#if !defined(NB_TTY_CHANNELS) 
# error: You must define NB_TTY_CHANNELS in the hard_config.h file
#endif

#if (NB_TTY_CHANNELS < 1)
# error: NB_TTY_CHANNELS cannot be smaller than 1!
#endif

#if !defined(NB_TIM_CHANNELS) 
# error: You must define NB_TIM_CHANNELS in the hard_config.h file
#endif

#if !defined(NB_NIC_CHANNELS) 
# error: You must define NB_NIC_CHANNELS in the hard_config.h file
#endif

#if !defined(NB_CMA_CHANNELS) 
# error: You must define NB_CMA_CHANNELS in the hard_config.h file
#endif

#if !defined(GIET_NO_HARD_CC)
# error: You must define GIET_NO_HARD_CC in the giet_config.h file
#endif

#if !defined ( GIET_NIC_MAC4 )
# error: You must define GIET_NIC_MAC4 in the giet_config.h file
#endif

#if !defined ( GIET_NIC_MAC2 )
# error: You must define GIET_NIC_MAC2 in the giet_config.h file
#endif

////////////////////////////////////////////////////////////////////////////
//        Extern variables
////////////////////////////////////////////////////////////////////////////

// allocated in tty0.c file.
extern sqt_lock_t _tty0_sqt_lock;

// allocated in mwr_driver.c file.
extern simple_lock_t  _coproc_lock[X_SIZE*Y_SIZE];
extern unsigned int   _coproc_type[X_SIZE*Y_SIZE];
extern unsigned int   _coproc_info[X_SIZE*Y_SIZE];
extern unsigned int   _coproc_mode[X_SIZE*Y_SIZE];
extern unsigned int   _coproc_error[X_SIZE*Y_SIZE];
extern unsigned int   _coproc_gtid[X_SIZE*Y_SIZE];


// allocated in tty_driver.c file.
extern unsigned int _tty_rx_full[NB_TTY_CHANNELS];
extern unsigned int _tty_rx_buf[NB_TTY_CHANNELS];

// allocated in kernel_init.c file
extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX]; 

////////////////////////////////////////////////////////////////////////////
//     Channel allocators for peripherals
//     (TTY[0] is reserved for kernel)
////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
unsigned int _tty_channel[NB_TTY_CHANNELS]  = {1};

__attribute__((section(".kdata")))
unsigned int _tim_channel_allocator    = 0;

__attribute__((section(".kdata")))
unsigned int _cma_channel[NB_CMA_CHANNELS]  = {0};

__attribute__((section(".kdata")))
unsigned int _nic_rx_channel_allocator = 0;

__attribute__((section(".kdata")))
unsigned int _nic_tx_channel_allocator = 0;

////////////////////////////////////////////////////////////////////////////
//     NIC_RX and NIC_TX kernel chbuf arrays 
////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
ker_chbuf_t  _nic_ker_rx_chbuf[NB_NIC_CHANNELS] __attribute__((aligned(64)));

__attribute__((section(".kdata")))
ker_chbuf_t  _nic_ker_tx_chbuf[NB_NIC_CHANNELS] __attribute__((aligned(64)));

////////////////////////////////////////////////////////////////////////////
// FBF related chbuf descriptors array, indexed by the CMA channel index.
// Physical addresses of these chbuf descriptors required for L2 cache sync.
// FBF status
////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
fbf_chbuf_t _fbf_chbuf[NB_CMA_CHANNELS] __attribute__((aligned(64)));

__attribute__((section(".kdata")))
unsigned long long _fbf_chbuf_paddr[NB_CMA_CHANNELS];

__attribute__((section(".kdata")))
buffer_status_t _fbf_status[NB_CMA_CHANNELS] __attribute__((aligned(64)));

////////////////////////////////////////////////////////////////////////////
//    Initialize the syscall vector with syscall handlers
// Note: This array must be synchronised with the define in file stdio.h
////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
const void * _syscall_vector[64] = 
{
    &_sys_proc_xyp,                  /* 0x00 */
    &_get_proctime,                  /* 0x01 */
    &_sys_tty_write,                 /* 0x02 */
    &_sys_tty_read,                  /* 0x03 */
    &_sys_tty_alloc,                 /* 0x04 */
    &_sys_tasks_status,              /* 0x05 */
    &_sys_ukn,                       /* 0x06 */
    &_sys_heap_info,                 /* 0x07 */
    &_sys_local_task_id,             /* 0x08 */
    &_sys_global_task_id,            /* 0x09 */ 
    &_sys_fbf_cma_alloc,             /* 0x0A */
    &_sys_fbf_cma_init_buf,          /* 0x0B */
    &_sys_fbf_cma_start,             /* 0x0C */
    &_sys_fbf_cma_display,           /* 0x0D */
    &_sys_fbf_cma_stop,              /* 0x0E */
    &_sys_task_exit,                 /* 0x0F */

    &_sys_procs_number,              /* 0x10 */
    &_sys_fbf_sync_write,            /* 0x11 */
    &_sys_fbf_sync_read,             /* 0x12 */
    &_sys_thread_id,                 /* 0x13 */
    &_sys_tim_alloc,                 /* 0x14 */
    &_sys_tim_start,                 /* 0x15 */ 
    &_sys_tim_stop,                  /* 0x16 */
    &_sys_kill_application,          /* 0x17 */
    &_sys_exec_application,          /* 0x18 */   
    &_sys_context_switch,            /* 0x19 */
    &_sys_vseg_get_vbase,            /* 0x1A */
    &_sys_vseg_get_length,           /* 0x1B */
    &_sys_xy_from_ptr,               /* 0x1C */
    &_sys_ukn,                       /* 0x1D */
    &_sys_ukn,                       /* 0x1E */
    &_sys_ukn,                       /* 0x1F */

    &_fat_open,                      /* 0x20 */
    &_fat_read,                      /* 0x21 */
    &_fat_write,                     /* 0x22 */
    &_fat_lseek,                     /* 0x23 */
    &_fat_file_info,                 /* 0x24 */
    &_fat_close,                     /* 0x25 */
    &_fat_remove,                    /* 0x26 */
    &_fat_rename,                    /* 0x27 */
    &_fat_mkdir,                     /* 0x28 */
    &_fat_opendir,                   /* 0x29 */
    &_fat_closedir,                  /* 0x2A */
    &_fat_readdir,                   /* 0x2B */
    &_sys_ukn,                       /* 0x2C */
    &_sys_ukn,                       /* 0x2D */
    &_sys_ukn,                       /* 0x2E */
    &_sys_ukn,                       /* 0x2F */

    &_sys_nic_alloc,                 /* 0x30 */
    &_sys_nic_start,                 /* 0x31 */
    &_sys_nic_move,                  /* 0x32 */
    &_sys_nic_stop,                  /* 0x33 */
    &_sys_nic_stats,                 /* 0x34 */
    &_sys_nic_clear,                 /* 0x35 */ 
    &_sys_ukn,                       /* 0x36 */
    &_sys_ukn,                       /* 0x37 */
    &_sys_ukn,                       /* 0x38 */   
    &_sys_ukn,                       /* 0x39 */
    &_sys_ukn,                       /* 0x3A */
    &_sys_coproc_completed,          /* 0x3B */
    &_sys_coproc_alloc,              /* 0x3C */
    &_sys_coproc_channel_init,       /* 0x3D */
    &_sys_coproc_run,                /* 0x3E */
    &_sys_coproc_release,            /* 0x3F */
};


//////////////////////////////////////////////////////////////////////////////
//           Applications related syscall handlers 
//////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
int _sys_kill_application( char* name )
{
    mapping_header_t * header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t * vspace  = _get_vspace_base(header);
    mapping_task_t   * task    = _get_task_base(header);

    unsigned int vspace_id;
    unsigned int task_id;
    unsigned int y_size = header->y_size;

#if GIET_DEBUG_EXEC
if ( _get_proctime() > GIET_DEBUG_EXEC )
_printf("\n[DEBUG EXEC] enters _sys_kill_application() for %s\n", name );
#endif

    // scan vspaces
    for (vspace_id = 0; vspace_id < header->vspaces; vspace_id++) 
    {
        if ( _strcmp( vspace[vspace_id].name, name ) == 0 ) 
        {
            // check if application can be killed
            if ( vspace[vspace_id].active ) return -2;

            // scan tasks in vspace
            for (task_id = vspace[vspace_id].task_offset; 
                 task_id < (vspace[vspace_id].task_offset + vspace[vspace_id].tasks); 
                 task_id++) 
            {
                unsigned int cid   = task[task_id].clusterid;
                unsigned int x     = cid / y_size;
                unsigned int y     = cid % y_size;
                unsigned int p     = task[task_id].proclocid;
                unsigned int ltid  = task[task_id].ltid;

                // get scheduler pointer for processor running the task
                static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];

                // set KILL signal bit
                _atomic_or( &psched->context[ltid][CTX_SIG_ID] , SIG_MASK_KILL );
            } 

#if GIET_DEBUG_EXEC 
if ( _get_proctime() > GIET_DEBUG_EXEC )
_printf("\n[DEBUG EXEC] exit _sys_kill_application() : %s will be killed\n", name );
#endif

            return 0;
        }
    } 

#if GIET_DEBUG_EXEC 
if ( _get_proctime() > GIET_DEBUG_EXEC )
_printf("\n[DEBUG EXEC] exit _sys_kill_application() : %s not found\n", name );
#endif

    return -1;    // not found 

}  // end _sys_kill_application()
    
///////////////////////////////////////
int _sys_exec_application( char* name )
{
    mapping_header_t * header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t * vspace  = _get_vspace_base(header);
    mapping_task_t   * task    = _get_task_base(header);

    unsigned int vspace_id;
    unsigned int task_id;
    unsigned int y_size = header->y_size;

#if GIET_DEBUG_EXEC 
if ( _get_proctime() > GIET_DEBUG_EXEC )
_printf("\n[DEBUG EXEC] enters _sys_exec_application() at cycle %d for %s\n",
         _get_proctime() , name );
#endif

    // scan vspaces
    for (vspace_id = 0 ; vspace_id < header->vspaces ; vspace_id++) 
    {
        if ( _strcmp( vspace[vspace_id].name, name ) == 0 ) 
        {
            // scan tasks in vspace
            for (task_id = vspace[vspace_id].task_offset; 
                 task_id < (vspace[vspace_id].task_offset + vspace[vspace_id].tasks); 
                 task_id++) 
            {
                unsigned int cid   = task[task_id].clusterid;
                unsigned int x     = cid / y_size;
                unsigned int y     = cid % y_size;
                unsigned int p     = task[task_id].proclocid;
                unsigned int ltid  = task[task_id].ltid;

                // get scheduler pointer for processor running the task
                static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];

                // set EXEC signal bit
                _atomic_or( &psched->context[ltid][CTX_SIG_ID] , SIG_MASK_EXEC );
            } 

#if GIET_DEBUG_EXEC 
if ( _get_proctime() > GIET_DEBUG_EXEC )
_printf("\n[DEBUG EXEC] exit _sys_exec_application() at cycle %d : %s will be executed\n",
        _get_proctime() , name );
#endif

            return 0;   // application found and activated
        }
    }

#if GIET_DEBUG_EXEC 
if ( _get_proctime() > GIET_DEBUG_EXEC )
_printf("\n[DEBUG EXEC] exit _sys_exec_application() at cycle %d : %s not found\n",
         _get_proctime() , name );
#endif

    return -1;    // not found 

}  // end _sys_exec_application()
    

//////////////////////////////////////////////////////////////////////////////
//           Coprocessors related syscall handlers 
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
int _sys_coproc_alloc( unsigned int   coproc_type,
                       unsigned int*  coproc_info )
{
    // In this implementation, the allocation policy is constrained:
    // the coprocessor must be in the same cluster as the calling task,
    // and there is at most one coprocessor per cluster

    mapping_header_t  * header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t * cluster = _get_cluster_base(header);
    mapping_periph_t  * periph  = _get_periph_base(header);

    // get cluster coordinates and cluster global index 
    unsigned int procid     = _get_procid();
    unsigned int x          = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y          = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    unsigned int cluster_id = x * Y_SIZE + y;
 
    // search coprocessor in cluster
    mapping_periph_t*  found = NULL;
    unsigned int min   = cluster[cluster_id].periph_offset;
    unsigned int max   = min + cluster[cluster_id].periphs;
    unsigned int periph_id;
    for ( periph_id = min ; periph_id < max ; periph_id++ )
    {
        if ( (periph[periph_id].type == PERIPH_TYPE_MWR) &&
             (periph[periph_id].subtype == coproc_type) )
        {
            found = &periph[periph_id];
            break;
        }
    } 

    if ( found != NULL )
    {
        // get the lock (at most one coproc per cluster)
        _simple_lock_acquire( &_coproc_lock[cluster_id] );

        // register coproc characteristics in kernel arrays
        _coproc_type[cluster_id] = coproc_type;
        _coproc_info[cluster_id] = (found->arg0 & 0xFF)     |
                                   (found->arg1 & 0xFF)<<8  |
                                   (found->arg2 & 0xFF)<<16 |
                                   (found->arg3 & 0xFF)<<24 ;

        // returns coprocessor info
        *coproc_info = _coproc_info[cluster_id];

        // register coprocessor coordinates in task context
        unsigned int cluster_xy = (x<<Y_WIDTH) + y;
        _set_context_slot( CTX_COPROC_ID , cluster_xy );

#if GIET_DEBUG_COPROC
_printf("\n[GIET DEBUG COPROC] _sys_coproc_alloc() in cluster[%d,%d]\n"
        "  coproc_info = %x / cluster_xy = %x\n",
        x , y , *coproc_info , cluster_xy );
#endif
        return 0;
    }
    else
    {
         _printf("\n[GIET_ERROR] in _sys_coproc_alloc(): no coprocessor "
                 " with type %d available in cluster[%d,%d]\n",
                 coproc_type , x , y );
        return -1;
    }
}  // end _sys_coproc_alloc()

////////////////////////////////////////////////////////
int _sys_coproc_release( unsigned int coproc_reg_index )
{
    // processor coordinates
    unsigned int procid = _get_procid();
    unsigned int x      = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y      = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    unsigned int p      = procid & ((1<<P_WIDTH)-1);
    
    // get coprocessor coordinates
    unsigned int cluster_xy = _get_context_slot( CTX_COPROC_ID );
    if ( cluster_xy > 0xFF )
    {
         _printf("\n[GIET_ERROR] in _sys_coproc_release(): "
                 "no coprocessor allocated to task running on P[%d,%d,%d]\n",
                 x , y , p ); 
         return -1;
    }

    unsigned int cx         = cluster_xy >> Y_WIDTH;
    unsigned int cy         = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int cluster_id = cx * Y_SIZE + cy;
    unsigned int info       = _coproc_info[cluster_id];
    unsigned int nb_to      = info & 0xFF;
    unsigned int nb_from    = (info>>8) & 0xFF;
    unsigned int channel;

    // stops coprocessor and communication channels
    _mwr_set_coproc_register( cluster_xy , coproc_reg_index , 0 );
    for ( channel = 0 ; channel < (nb_from + nb_to) ; channel++ )
    {
        _mwr_set_channel_register( cluster_xy , channel , MWR_CHANNEL_RUNNING , 0 );
    }

    // deallocates coprocessor coordinates in task context
    _set_context_slot( CTX_COPROC_ID , 0xFFFFFFFF );

    // release coprocessor lock
    _simple_lock_release( &_coproc_lock[cluster_id] );

#if GIET_DEBUG_COPROC
_printf("\n[GIET DEBUG COPROC] _sys_coproc_release() in cluster[%d,%d]\n",
        cx, cy );
#endif

    return 0;
}  // end _sys_coproc_release()

//////////////////////////////////////////////////////////////
int _sys_coproc_channel_init( unsigned int            channel,
                              giet_coproc_channel_t*  desc )
{
    // processor coordinates
    unsigned int procid = _get_procid();
    unsigned int x      = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y      = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    unsigned int p      = procid & ((1<<P_WIDTH)-1);
    
    // get coprocessor coordinates
    unsigned int cluster_xy = _get_context_slot( CTX_COPROC_ID );
    if ( cluster_xy > 0xFF )
    {
         _printf("\n[GIET_ERROR] in _sys_coproc_channel_init(): "
                 "no coprocessor allocated to task running on P[%d,%d,%d]\n",
                 x , y , p ); 
         return -1;
    }

    // check channel mode
    unsigned mode = desc->channel_mode;
    if ( (mode != MODE_MWMR) && 
         (mode != MODE_DMA_IRQ) && 
         (mode != MODE_DMA_NO_IRQ) )
    {
         _printf("\n[GIET_ERROR] in _sys_coproc_channel_init(): "
                 " illegal mode\n");
         return -1;
    }

    // get memory buffer size
    unsigned int size = desc->buffer_size;
 
    // physical addresses
    unsigned long long buffer_paddr;
    unsigned int       buffer_lsb;
    unsigned int       buffer_msb;
    unsigned long long mwmr_paddr = 0;
    unsigned int       mwmr_lsb;
    unsigned int       mwmr_msb;
    unsigned long long lock_paddr = 0;
    unsigned int       lock_lsb;
    unsigned int       lock_msb;

    unsigned int       flags;     // unused

    // compute memory buffer physical address
    buffer_paddr = _v2p_translate( desc->buffer_vaddr , &flags );
    buffer_lsb   = (unsigned int)buffer_paddr;
    buffer_msb   = (unsigned int)(buffer_paddr>>32); 

    // call MWMR_DMA driver
    _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_MODE, mode ); 
    _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_SIZE, size ); 
    _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_BUFFER_LSB, buffer_lsb ); 
    _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_BUFFER_MSB, buffer_msb ); 
                       
    if ( mode == MODE_MWMR )
    {
        // compute MWMR descriptor physical address
        mwmr_paddr = _v2p_translate( desc->mwmr_vaddr , &flags );
        mwmr_lsb = (unsigned int)mwmr_paddr;
        mwmr_msb = (unsigned int)(mwmr_paddr>>32); 

        // call MWMR_DMA driver
        _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_MWMR_LSB, mwmr_lsb ); 
        _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_MWMR_MSB, mwmr_msb ); 

        // compute lock physical address
        lock_paddr = _v2p_translate( desc->lock_vaddr , &flags );
        lock_lsb = (unsigned int)lock_paddr;
        lock_msb = (unsigned int)(lock_paddr>>32); 

        // call MWMR_DMA driver
        _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_LOCK_LSB, lock_lsb ); 
        _mwr_set_channel_register( cluster_xy, channel, MWR_CHANNEL_LOCK_MSB, lock_msb ); 
    }

#if GIET_DEBUG_COPROC
_printf("\n[GIET DEBUG COPROC] _sys_coproc_channel_init() for coproc[%d,%d]\n"
        " channel =  %d / mode = %d / buffer_size = %d\n"
        " buffer_paddr = %l / mwmr_paddr = %l / lock_paddr = %l\n",
        x , y , channel , mode , size ,
        buffer_paddr, mwmr_paddr, lock_paddr );
#endif
        
    return 0;
} // end _sys_coproc_channel_init()

////////////////////////////////////////////////////
int _sys_coproc_run( unsigned int coproc_reg_index )
{
    // processor coordinates
    unsigned int procid = _get_procid();
    unsigned int x      = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y      = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    unsigned int p      = procid & ((1<<P_WIDTH)-1);
    
    // get coprocessor coordinates
    unsigned int cluster_xy = _get_context_slot( CTX_COPROC_ID );
    if ( cluster_xy > 0xFF )
    {
         _printf("\n[GIET_ERROR] in _sys_coproc_run(): "
                 "no coprocessor allocated to task running on P[%d,%d,%d]\n",
                 x , y , p ); 
         return -1;
    }

    unsigned int cx         = cluster_xy >> Y_WIDTH;
    unsigned int cy         = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int cluster_id = cx * Y_SIZE + cy;
    unsigned int info       = _coproc_info[cluster_id];
    unsigned int nb_to      = info & 0xFF;
    unsigned int nb_from    = (info>>8) & 0xFF;
    unsigned int mode       = 0xFFFFFFFF;
    unsigned int channel;

    // register coprocessor running mode 
    for ( channel = 0 ; channel < (nb_from + nb_to) ; channel++ )
    {
        unsigned int temp;
        temp = _mwr_get_channel_register( cluster_xy , channel , MWR_CHANNEL_MODE );

        if ( mode == 0xFFFFFFFF ) 
        {
            mode = temp;
        }
        else if ( temp != mode )
        {
            _printf("\n[GIET_ERROR] P[%d,%d,%d] in _sys_coproc_run() for coprocessor[%d,%d]\n"
                    "  all channels don't have the same mode\n", x , y , p , cx , cy );
            return -1;
        }
    }
    _coproc_mode[cluster_id] = mode;

    // start all communication channels
    for ( channel = 0 ; channel < (nb_from + nb_to) ; channel++ )
    {
        _mwr_set_channel_register( cluster_xy , channel , MWR_CHANNEL_RUNNING , 1 );
    }

    //////////////////////////////////////////////////////////////////////////
    if ( (mode == MODE_MWMR) || (mode == MODE_DMA_NO_IRQ) )  // no descheduling
    {
        // start coprocessor
        _mwr_set_coproc_register( cluster_xy , coproc_reg_index , 1 );

#if GIET_DEBUG_COPROC
if ( mode == MODE_MWMR )
_printf("\n[GIET DEBUG COPROC] _sys_coproc_run() P[%d,%d,%d] starts coprocessor[%d,%d]\n"
        "   MODE_MWMR at cycle %d\n", x , y , p , cx , cy , _get_proctime() );
else
_printf("\n[GIET DEBUG COPROC] _sys_coproc_run() P[%d,%d,%d] starts coprocessor[%d,%d]\n"
        "   MODE_DMA_NO_IRQ at cycle %d\n", x , y , p , cx , cy , _get_proctime() );
#endif

        return 0;
    }
    ///////////////////////////////////////////////////////////////////////////
    else                                // mode == MODE_DMA_IRQ => descheduling
    {
        // set _coproc_gtid 
        unsigned int ltid = _get_current_task_id();
        _coproc_gtid[cluster_id] = (procid<<16) + ltid;

        // enters critical section
        unsigned int save_sr;
        _it_disable( &save_sr ); 

        // set NORUN_MASK_COPROC bit
        static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
        unsigned int*       ptr     = &psched->context[ltid][CTX_NORUN_ID];
        _atomic_or( ptr , NORUN_MASK_COPROC );

        // start coprocessor
        _mwr_set_coproc_register( cluster_xy , coproc_reg_index , 1 );

#if GIET_DEBUG_COPROC
_printf("\n[GIET DEBUG COPROC] _sys_coproc_run() P[%d,%d,%d] starts coprocessor[%d,%d]\n"
        "   MODE_DMA_IRQ at cycle %d\n", x , y , p , cx , cy , _get_proctime() );
#endif

        // deschedule task
        _ctx_switch(); 

#if GIET_DEBUG_COPROC
_printf("\n[GIET DEBUG COPROC] _sys_coproc_run() P[%d,%d,%d] resume\n"
        "  coprocessor[%d,%d] completion at cycle %d\n", 
        x , y , p , cx , cy , _get_proctime() );
#endif

        // restore SR
        _it_restore( &save_sr );

        // return error computed by mwr_isr()
        return _coproc_error[cluster_id];
    } 
} // end _sys_coproc_run()

///////////////////////////
int _sys_coproc_completed()
{
    // processor coordinates
    unsigned int procid = _get_procid();
    unsigned int x      = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y      = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    unsigned int p      = procid & ((1<<P_WIDTH)-1);
    
    // get coprocessor coordinates
    unsigned int cluster_xy = _get_context_slot( CTX_COPROC_ID );
    if ( cluster_xy > 0xFF )
    {
         _printf("\n[GIET_ERROR] in _sys_coproc_completed(): "
                 "no coprocessor allocated to task running on P[%d,%d,%d]\n",
                 x , y , p ); 
         return -1;
    }

    unsigned int cx         = cluster_xy >> Y_WIDTH;
    unsigned int cy         = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int cluster_id = cx * Y_SIZE + cy;
    unsigned int mode       = _coproc_mode[cluster_id];

    // analyse possible errors
    if ( mode == MODE_DMA_NO_IRQ )
    {
        unsigned int info       = _coproc_info[cluster_id];
        unsigned int nb_to      = info & 0xFF;
        unsigned int nb_from    = (info>>8) & 0xFF;
        unsigned int error      = 0;
        unsigned int channel;
        unsigned int status;

        // get status for all channels, and signal all reported errors
        for ( channel = 0 ; channel < (nb_to +nb_from) ; channel++ )
        {
            do
            {
                status = _mwr_get_channel_register( cluster_xy , channel , MWR_CHANNEL_STATUS );
                if ( status == MWR_CHANNEL_ERROR_DATA )
                {
                    _printf("\n[GIET_ERROR] in _sys_coproc_completed()"
                            " / channel %d / DATA_ERROR\n", channel );
                    error = 1;
                    break;
                }
                else if ( status == MWR_CHANNEL_ERROR_LOCK )
                {
                    _printf("\n[GIET_ERROR] in _sys_coproc_completed()"
                            " / channel %d / LOCK_ERROR\n", channel );
                    error = 1;
                    break;
                }
                else if ( status == MWR_CHANNEL_ERROR_DESC )
                {
                    _printf("\n[GIET_ERROR] in _sys_coproc_completed()"
                            " / channel %d / DESC_ERROR\n", channel );
                    error = 1;
                    break;
                }
            } while ( status == MWR_CHANNEL_BUSY );

            // reset channel
            _mwr_set_channel_register( cluster_xy , channel , MWR_CHANNEL_RUNNING , 0 ); 

        }  // end for channels

#if GIET_DEBUG_COPROC
_printf("\n[GIET DEBUG COPROC] _sys_coproc_completed() for coprocessor[%d,%d] error = %d\n", 
        cx , cy , error );
#endif

        return error;
    }
    else  // mode == MODE_MWMR or MODE_DMA_IRQ
    {
        _printf("\n[GIET ERROR] sys_coproc_completed() should not be called for "
                "coprocessor[%d,%d] running in MODE_MWMR or MODE_DMA_IRQ\n", cx , cy );
        return 1;
    }
} // end _sys_coproc_completed()


//////////////////////////////////////////////////////////////////////////////
//             TTY related syscall handlers 
//////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////
int _sys_tty_alloc( unsigned int shared )
{
    unsigned int channel;

    if ( _get_context_slot( CTX_TTY_ID ) < NB_TTY_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_tty_alloc() : TTY channel already allocated\n");
        return 0;
    }

    // get a new TTY channel
    for ( channel = 0 ; channel < NB_TTY_CHANNELS ; channel++ )
    {
        if ( !_tty_channel[channel] )
        {
            _tty_channel[channel] = 1;
            break;
        }
    }
    if ( channel >= NB_TTY_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_tty_alloc() : no TTY channel available\n");
        return -1;
    }

    // reset kernel buffer for allocated TTY channel
    _tty_rx_full[channel] = 0;

    // allocate a WTI mailbox to the calling proc if external IRQ
    unsigned int unused;
    if ( USE_PIC ) _ext_irq_alloc( ISR_TTY_RX , channel , &unused ); 
   
    // update CTX_TTY_ID 
    if ( shared )         // for all tasks in the same vspace
    {
        unsigned int      vspace_id = _get_context_slot( CTX_VSID_ID );
        mapping_header_t  *header   = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
        mapping_vspace_t  *vspace   = _get_vspace_base(header);
        mapping_task_t    *task     = _get_task_base(header);

        // scan tasks in vspace
        unsigned int task_id;
        for (task_id = vspace[vspace_id].task_offset;
             task_id < (vspace[vspace_id].task_offset + vspace[vspace_id].tasks);
             task_id++)
        {
            unsigned int y_size        = header->y_size;
            unsigned int cid           = task[task_id].clusterid;
            unsigned int x             = cid / y_size;
            unsigned int y             = cid % y_size;
            unsigned int p             = task[task_id].proclocid;
            unsigned int ltid          = task[task_id].ltid;
            static_scheduler_t* psched = (static_scheduler_t*)_schedulers[x][y][p];

            // don't overwrite TTY_ID
            if ( psched->context[ltid][CTX_TTY_ID] >= NB_TTY_CHANNELS )
            {
                psched->context[ltid][CTX_TTY_ID] = channel;
            }
        }
    }
    else                  // for calling task only
    {
        _set_context_slot( CTX_TTY_ID, channel );
    }

    return 0;
}

/////////////////////////////////////////
// NOTE: not a syscall
int _sys_tty_release()
{
    unsigned int channel = _get_context_slot( CTX_TTY_ID );

    if ( channel == -1 )
    {
        _printf("\n[GIET_ERROR] in _sys_tty_release() : TTY channel already released\n");
        return -1;
    }

    // release WTI mailbox
    if ( USE_PIC ) _ext_irq_release( ISR_TTY_RX , channel );

    // reset CTX_TTY_ID for all tasks in vspace
    unsigned int      vspace_id = _get_context_slot( CTX_VSID_ID );
    mapping_header_t  *header   = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t  *vspace   = _get_vspace_base(header);
    mapping_task_t    *task     = _get_task_base(header);

    unsigned int task_id;
    for (task_id = vspace[vspace_id].task_offset;
         task_id < (vspace[vspace_id].task_offset + vspace[vspace_id].tasks);
         task_id++)
    {
        unsigned int y_size        = header->y_size;
        unsigned int cid           = task[task_id].clusterid;
        unsigned int x             = cid / y_size;
        unsigned int y             = cid % y_size;
        unsigned int p             = task[task_id].proclocid;
        unsigned int ltid          = task[task_id].ltid;
        static_scheduler_t* psched = (static_scheduler_t*)_schedulers[x][y][p];

        // only clear matching TTY_ID
        if ( psched->context[ltid][CTX_TTY_ID] == channel )
        {
            psched->context[ltid][CTX_TTY_ID] = -1;
        }
    }

    // release TTY channel
    _tty_channel[channel] = 0;

    return 0;
}

/////////////////////////////////////////////////
int _sys_tty_write( const char*  buffer,    
                    unsigned int length,    // number of characters
                    unsigned int channel)   // channel index 
{
    unsigned int  nwritten;

    // compute and check tty channel
    if( channel == 0xFFFFFFFF )  channel = _get_context_slot(CTX_TTY_ID);
    if( channel >= NB_TTY_CHANNELS ) return -1;

    // write string to TTY channel
    for (nwritten = 0; nwritten < length; nwritten++) 
    {
        // check tty's status 
        if ( _tty_get_register( channel, TTY_STATUS ) & 0x2 )  break;

        // write one byte
        if (buffer[nwritten] == '\n') 
        {
            _tty_set_register( channel, TTY_WRITE, (unsigned int)'\r' );
        }
        _tty_set_register( channel, TTY_WRITE, (unsigned int)buffer[nwritten] );
    }
    
    return nwritten;
}

////////////////////////////////////////////////
int _sys_tty_read( char*        buffer, 
                   unsigned int length,    // unused
                   unsigned int channel)   // channel index
{
    // compute and check tty channel
    if( channel == 0xFFFFFFFF )  channel = _get_context_slot(CTX_TTY_ID);
    if( channel >= NB_TTY_CHANNELS ) return -1;

    // read one character from TTY channel
    if (_tty_rx_full[channel] == 0) 
    {
        return 0;
    }
    else 
    {
        *buffer = _tty_rx_buf[channel];
        _tty_rx_full[channel] = 0;
        return 1;
    }
}

///////////////////////////////////////////
int _sys_tty_get_lock( unsigned int   channel,       // unused
                       unsigned int * save_sr_ptr )
{
    // check tty channel
    if( channel != 0 )  return 1;

    _it_disable( save_sr_ptr );
    _sqt_lock_acquire( &_tty0_sqt_lock );
    return 0;
}

///////////////////////////////////////////////
int _sys_tty_release_lock( unsigned int   channel,
                           unsigned int * save_sr_ptr )
{
    // check tty channel
    if( channel != 0 )  return 1;

    _sqt_lock_release( &_tty0_sqt_lock );
    _it_restore( save_sr_ptr );
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//             TIM related syscall handlers 
//////////////////////////////////////////////////////////////////////////////

////////////////////
int _sys_tim_alloc()
{
    // get a new timer index 
    unsigned int channel = _atomic_increment( &_tim_channel_allocator, 1 );

    if ( channel >= NB_TIM_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_tim_alloc() : not enough TIM channels\n");
        return -1;
    }
    else
    {
        _set_context_slot( CTX_TIM_ID, channel );
        return 0;
    }
}

////////////////////
// NOTE: not a syscall
int _sys_tim_release()
{
    // release one timer
    _atomic_increment( &_tim_channel_allocator, 0xFFFFFFFF );

    return 0;
}

/////////////////////////////////////////
int _sys_tim_start( unsigned int period )
{
    // get timer index
    unsigned int channel = _get_context_slot( CTX_TIM_ID );
    if ( channel >= NB_TIM_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_tim_start() : not enough TIM channels\n");
        return -1;
    }

    // start timer
    _timer_start( channel, period );

    return 0;
}

///////////////////
int _sys_tim_stop()
{
    // get timer index
    unsigned int channel = _get_context_slot( CTX_TIM_ID );
    if ( channel >= NB_TIM_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_tim_stop() : illegal timer index\n");
        return -1;
    }

    // stop timer
    _timer_stop( channel );

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//             NIC related syscall handlers 
//////////////////////////////////////////////////////////////////////////////

#define NIC_CONTAINER_SIZE 4096

////////////////////////////////////////
int _sys_nic_alloc( unsigned int is_rx,
                    unsigned int xmax,
                    unsigned int ymax )
{
    // check xmax / ymax parameters
    if ( xmax > X_SIZE )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_alloc() xmax argument too large\n");
        return -1;
    }
    if ( ymax > Y_SIZE )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_alloc() ymax argument too large\n");
        return -1;
    }

    ////////////////////////////////////////////////////////
    // Step 1: get and register CMA and NIC channel index //
    ////////////////////////////////////////////////////////

    // get a NIC_RX or NIC_TX channel index 
    unsigned int nic_channel;
    unsigned int cma_channel;

    if ( is_rx ) nic_channel = _atomic_increment( &_nic_rx_channel_allocator, 1 );
    else         nic_channel = _atomic_increment( &_nic_tx_channel_allocator, 1 );

    if ( (nic_channel >= NB_NIC_CHANNELS) )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_alloc() not enough NIC channels\n");
        return -1;
    }

    // get a CMA channel index
    for ( cma_channel = 0 ; cma_channel < NB_CMA_CHANNELS ; cma_channel++ )
    {
        if ( !_cma_channel[cma_channel] )
        {
            _cma_channel[cma_channel] = 1;
            break;
        }
    }

    if ( cma_channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_alloc() not enough CMA channels\n");
        return -1;
    }

#if GIET_DEBUG_NIC
unsigned int thread  = _get_context_slot( CTX_TRDID_ID );
_printf("\n[GIET DEBUG NIC] Task %d enters sys_nic_alloc() at cycle %d\n"
        "  nic_channel = %d / cma_channel = %d\n",
        thread , _get_proctime() , nic_channel , cma_channel );
#endif

    // register nic_index and cma_index in task context
    if ( is_rx )
    {
        _set_context_slot( CTX_NIC_RX_ID, nic_channel );
        _set_context_slot( CTX_CMA_RX_ID, cma_channel );
    }
    else
    {
        _set_context_slot( CTX_NIC_TX_ID, nic_channel );
        _set_context_slot( CTX_CMA_TX_ID, cma_channel );
    }

    /////////////////////////////////////////////////////////////////////////////////
    // Step 2: loop on all the clusters                                            //
    // Allocate the kernel containers and status, compute the container and the    //
    // status physical addresses, fill and synchronize the kernel CHBUF descriptor //
    /////////////////////////////////////////////////////////////////////////////////

    // physical addresses to be registered in the CMA registers
    unsigned long long nic_chbuf_pbase;     // NIC chbuf physical address
    unsigned long long ker_chbuf_pbase;     // kernel chbuf physical address

    // allocate one kernel container and one status variable per cluster in the
    // (xmax / ymax) mesh
    unsigned int        cx;                 // cluster X coordinate
    unsigned int        cy;                 // cluster Y coordinate
    unsigned int        index;              // container index in chbuf
    unsigned int        vaddr;              // virtual address
    unsigned long long  cont_paddr;         // container physical address
    unsigned long long  sts_paddr;          // container status physical address

    unsigned int        flags;              // for _v2p_translate()

    for ( cx = 0 ; cx < xmax ; cx++ )
    {
        for ( cy = 0 ; cy < ymax ; cy++ )
        {
            // compute index in chbuf
            index = (cx * ymax) + cy; 

            // allocate the kernel container
            vaddr = (unsigned int)_remote_malloc( NIC_CONTAINER_SIZE, cx, cy );

            if ( vaddr == 0 )  // not enough kernel heap memory in cluster[cx,cy]
            {
                _printf("\n[GIET_ERROR] in _sys_nic_alloc() not enough kenel heap"
                        " in cluster[%d,%d]\n", cx, cy );
                return -1;
            }

            // compute container physical address
            cont_paddr = _v2p_translate( vaddr , &flags );

            // checking container address alignment
            if ( cont_paddr & 0x3F )
            {
                _printf("\n[GIET ERROR] in _sys_nic_alloc() : container address of cluster[%d,%d] not aligned\n",
                        cx, cy);
                return -1;
            }

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_alloc()\n"
        " allocates in cluster[%d,%d]:\n"
        " - container vaddr = %x / paddr = %l\n",
        thread , cx , cy , vaddr, cont_paddr );
#endif

            // allocate the kernel container status
            // it occupies 64 bytes but only last bit is useful (1 for full and 0 for empty)
            vaddr = (unsigned int)_remote_malloc( 64, cx, cy );

            if ( vaddr == 0 )  // not enough kernel heap memory in cluster[cx,cy]
            {
                _printf("\n[GIET_ERROR] in _sys_nic_alloc() not enough kenel heap"
                        " in cluster[%d,%d]\n", cx, cy );
                return -1;
            }

            // compute status physical address
            sts_paddr = _v2p_translate( vaddr , &flags );

            // checking status address alignment
            if ( sts_paddr & 0x3F )
            {
                _printf("\n[GIET ERROR] in _sys_nic_alloc() : status address of cluster[%d,%d] not aligned\n",
                        cx, cy);
                return -1;
            }

            // initialize chbuf entry
            // The buffer descriptor has the following structure:
            // - the 26 LSB bits contain bits[6:31] of the buffer physical address
            // - the 26 following bits contain bits[6:31] of the physical address where the
            //   buffer status is located
            // - the 12 MSB bits contain the common address extension of the buffer and its
            //   status
            if ( is_rx )
                _nic_ker_rx_chbuf[nic_channel].buf_desc[index] =
                    (unsigned long long)
                    ((sts_paddr & 0xFFFFFFFFULL) >> 6) +
                    (((cont_paddr & 0xFFFFFFFFFFFULL) >> 6) << 26);
            else
                _nic_ker_tx_chbuf[nic_channel].buf_desc[index] =
                    (unsigned long long)
                    ((sts_paddr & 0xFFFFFFC0ULL) >> 6) +
                    (((cont_paddr & 0xFFFFFFFFFC0ULL) >> 6) << 26);

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_alloc()\n"
        " - status vaddr = %x / paddr = %l\n"
        " Buffer descriptor = %l\n",
        thread, vaddr, sts_paddr,
        (unsigned long long)((sts_paddr & 0xFFFFFFFFULL) >> 6) + 
        (((cont_paddr & 0xFFFFFFFFFFFULL) >> 6) << 26) );
#endif
        }
    }

    // complete kernel chbuf initialisation
    if ( is_rx )
    {
        _nic_ker_rx_chbuf[nic_channel].xmax = xmax;
        _nic_ker_rx_chbuf[nic_channel].ymax = ymax;
    }
    else
    {
        _nic_ker_tx_chbuf[nic_channel].xmax = xmax;
        _nic_ker_tx_chbuf[nic_channel].ymax = ymax;
    }

    // compute the kernel chbuf descriptor physical address
    if ( is_rx ) vaddr = (unsigned int)( &_nic_ker_rx_chbuf[nic_channel] );
    else         vaddr = (unsigned int)( &_nic_ker_tx_chbuf[nic_channel] );

    ker_chbuf_pbase = _v2p_translate( vaddr , &flags );

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_alloc() get kernel chbuf\n"
        "  vaddr = %x / paddr = %l\n",
        thread , vaddr , ker_chbuf_pbase );
#endif

    // sync the kernel chbuf in L2 after write in L2
    _mmc_sync( ker_chbuf_pbase, sizeof( ker_chbuf_t ) );

    ///////////////////////////////////////////////////////////////
    // Step 3: compute the NIC chbuf descriptor physical address //
    ///////////////////////////////////////////////////////////////

    unsigned int offset;
    if ( is_rx ) offset = 0x4100;
    else         offset = 0x4110;
    nic_chbuf_pbase = (((unsigned long long)((X_IO << Y_WIDTH) + Y_IO))<<32) |
                      (SEG_NIC_BASE + (nic_channel<<15) + offset);

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_alloc() get NIC chbuf : paddr = %l\n",
        thread , nic_chbuf_pbase );
#endif

    ////////////////////////////////////////////////////////////////////////////////
    // Step 4: initialize CMA registers defining the source & destination chbufs //
    ////////////////////////////////////////////////////////////////////////////////

    if ( is_rx )               // NIC to kernel
    {
        _cma_set_register( cma_channel, CHBUF_SRC_DESC , (unsigned int)(nic_chbuf_pbase) );
        _cma_set_register( cma_channel, CHBUF_SRC_EXT  , (unsigned int)(nic_chbuf_pbase>>32) );
        _cma_set_register( cma_channel, CHBUF_SRC_NBUFS, 2 );
        _cma_set_register( cma_channel, CHBUF_DST_DESC , (unsigned int)(ker_chbuf_pbase) );
        _cma_set_register( cma_channel, CHBUF_DST_EXT  , (unsigned int)(ker_chbuf_pbase>>32) );
        _cma_set_register( cma_channel, CHBUF_DST_NBUFS, xmax * ymax );
    }
    else                      // kernel to NIC
    {
        _cma_set_register( cma_channel, CHBUF_SRC_DESC , (unsigned int)(ker_chbuf_pbase) );
        _cma_set_register( cma_channel, CHBUF_SRC_EXT  , (unsigned int)(ker_chbuf_pbase>>32) );
        _cma_set_register( cma_channel, CHBUF_SRC_NBUFS, xmax * ymax );
        _cma_set_register( cma_channel, CHBUF_DST_DESC , (unsigned int)(nic_chbuf_pbase) );
        _cma_set_register( cma_channel, CHBUF_DST_EXT  , (unsigned int)(nic_chbuf_pbase>>32) );
        _cma_set_register( cma_channel, CHBUF_DST_NBUFS, 2 );
    }

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d exit _sys_nic_alloc() at cycle %d\n",
        thread, _get_proctime() );
#endif

    return nic_channel;
} // end _sys_nic_alloc()


////////////////////////////////////////
// NOTE: not a syscall
int _sys_nic_release( unsigned int is_rx )
{
    if ( is_rx )
        _atomic_increment( &_nic_rx_channel_allocator , 0xFFFFFFFF );
    else
        _atomic_increment( &_nic_tx_channel_allocator , 0xFFFFFFFF );

    return 0;
}


////////////////////////////////////////
int _sys_nic_start( unsigned int is_rx,
                    unsigned int channel )
{
    unsigned int nic_channel;
    unsigned int cma_channel;

    // get NIC channel index and CMA channel index from task context
    if ( is_rx )
    {
        nic_channel = _get_context_slot( CTX_NIC_RX_ID );
        cma_channel = _get_context_slot( CTX_CMA_RX_ID );
    }
    else
    {
        nic_channel = _get_context_slot( CTX_NIC_TX_ID );
        cma_channel = _get_context_slot( CTX_CMA_TX_ID );
    }

#if GIET_DEBUG_NIC
unsigned int thread  = _get_context_slot( CTX_TRDID_ID );
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_start() at cycle %d\n"
        "  get NIC channel = %d / CMA channel = %d\n",
        thread, _get_proctime(), nic_channel, cma_channel );
#endif

    // check NIC and CMA channels index
    if ( nic_channel != channel )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_start(): illegal NIC channel\n");
        return -1;
    }
    if ( cma_channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_start(): illegal CMA channel\n");
        return -1;
    }

    // start CMA transfer
    _cma_set_register( cma_channel, CHBUF_BUF_SIZE , NIC_CONTAINER_SIZE );
    _cma_set_register( cma_channel, CHBUF_PERIOD   , 0 );     // OUT_OF_ORDER 
    _cma_set_register( cma_channel, CHBUF_RUN      , 1 );

    // activates NIC channel
    _nic_channel_start( nic_channel, is_rx, GIET_NIC_MAC4, GIET_NIC_MAC2 );

#if GIET_DEBUG_NIC
    _printf("\n[GIET DEBUG NIC] Task %d exit _sys_nic_start() at cycle %d\n",
            thread , _get_proctime() );
#endif

    return 0;
}  // end _sys_nic_start()


//////////////////////////////////////
int _sys_nic_move( unsigned int is_rx,
                   unsigned int channel,
                   void*        buffer )
{

#if GIET_DEBUG_NIC
unsigned int thread  = _get_context_slot( CTX_TRDID_ID );
_printf("\n[GIET DEBUG NIC] Task %d enters _sys_nic_move() at cycle %d\n",
        thread , _get_proctime() );
#endif

    // check NIC channel index
    if ( channel >= NB_NIC_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_move() : illegal NIC channel index\n");
        return -1;
    }

    // get kernel chbuf virtual address
    ker_chbuf_t* ker_chbuf;
    if ( is_rx )  ker_chbuf = &_nic_ker_rx_chbuf[channel];
    else          ker_chbuf = &_nic_ker_tx_chbuf[channel];

    // get xmax / ymax parameters
    unsigned int xmax = ker_chbuf->xmax;
    unsigned int ymax = ker_chbuf->ymax;

    // get cluster coordinates for the processor running the calling task
    unsigned int  procid = _get_procid();
    unsigned int  cx     = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int  cy     = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    
    // check processor coordinates / (xmax,ymax)
    if ( cx >= xmax )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_move() : processor X coordinate = %d"
                " / xmax = %d\n", cx , xmax );
        return -1;
    }
    if ( cy >= ymax )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_move() : processor Y coordinate = %d"
                " / ymax = %d\n", cy , ymax );
        return -1;
    }
    
    unsigned long long usr_buf_paddr;       // user buffer physical address 
    unsigned long long ker_buf_paddr;       // kernel buffer physical address
    unsigned long long ker_sts_paddr;       // kernel buffer status physical address
    unsigned long long ker_buf_desc;        // kernel buffer descriptor
    unsigned int       ker_sts;             // kernel buffer status (full or empty)
    unsigned int       index;               // kernel buffer index in chbuf
    unsigned int       flags;               // for _v2P_translate

    // Compute user buffer physical address and check access rights
    usr_buf_paddr = _v2p_translate( (unsigned int)buffer , &flags );

    if ( (flags & PTE_U) == 0 )
    {
        _printf("\n[GIET ERROR] in _sys_nic_tx_move() : illegal user buffer address\n");
        return -1;
    }

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_move() get user buffer : paddr = %l\n",
        thread, usr_buf_paddr );
#endif

    // compute buffer index, buffer descriptor paddr and buffer status paddr
    index = (ymax * cx) + cy;
    ker_buf_desc = ker_chbuf->buf_desc[index];
    ker_sts_paddr = ((ker_buf_desc & 0xFFF0000000000000ULL) >> 20) +
                    ((ker_buf_desc & 0x3FFFFFFULL) << 6);

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_move() read ker_buf_desc %d at cycle %d\n"
        "  kernel buffer descriptor = %l\n",
        thread, index, _get_proctime(), ker_buf_desc );
#endif

    // poll local kernel container status until success
    while ( 1 )
    {
        // inval buffer descriptor in L2 before read in L2
        _mmc_inval( ker_sts_paddr, 4 );
        ker_sts = _physical_read( ker_sts_paddr );

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_move() read ker_buf_sts %d at cycle %d\n"
        "  paddr = %l / kernel buffer status = %x\n",
        thread, index, _get_proctime(), ker_sts_paddr, ker_sts );
#endif

        // test buffer status and break if found
        if ( ( is_rx != 0 ) && ( ker_sts == 0x1 ) ) break;
        if ( ( is_rx == 0 ) && ( ker_sts == 0 ) ) break;
    }

    // compute kernel buffer physical address
    ker_buf_paddr = (ker_buf_desc & 0xFFFFFFFFFC000000ULL) >> 20;
    
    // move one container
    if ( is_rx )              // RX transfer
    {
        // inval kernel buffer in L2 before read in L2
        _mmc_inval( ker_buf_paddr, NIC_CONTAINER_SIZE );

        // transfer data from kernel buffer to user buffer
        _physical_memcpy( usr_buf_paddr, 
                          ker_buf_paddr, 
                          NIC_CONTAINER_SIZE );
#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_move() transfer kernel buffer %l\n"
        " to user buffer %l at cycle %d\n",
        thread , ker_buf_paddr , usr_buf_paddr , _get_proctime() );
#endif

    }
    else                      // TX transfer
    {
        // transfer data from user buffer to kernel buffer
        _physical_memcpy( ker_buf_paddr, 
                          usr_buf_paddr, 
                          NIC_CONTAINER_SIZE );

        // sync kernel buffer in L2 after write in L2
        _mmc_sync( ker_buf_paddr, NIC_CONTAINER_SIZE );

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d in _sys_nic_move() transfer "
        "user buffer %l to kernel buffer %l at cycle %d\n",
        thread , usr_buf_paddr , ker_buf_paddr , _get_proctime() );
#endif

    }

    // update kernel chbuf status 
    if ( is_rx ) _physical_write ( ker_sts_paddr, 0 );
    else         _physical_write ( ker_sts_paddr, 0x1 );

    // sync kernel chbuf in L2 after write in L2
    _mmc_sync( ker_sts_paddr, 4 );

#if GIET_DEBUG_NIC
_printf("\n[GIET DEBUG NIC] Task %d get buffer %d  and exit _sys_nic_move() at cycle %d\n",
        thread , index , _get_proctime() );
#endif

    return 0;
} // end _sys_nic_move()


////////////////////////////////////////
int _sys_nic_stop( unsigned int is_rx,
                   unsigned int channel )
{
    unsigned int nic_channel;
    unsigned int cma_channel;

    // get NIC channel index and CMA channel index
    if ( is_rx )
    {
        nic_channel = _get_context_slot( CTX_NIC_RX_ID );
        cma_channel = _get_context_slot( CTX_CMA_RX_ID );
    }
    else
    {
        nic_channel = _get_context_slot( CTX_NIC_TX_ID );
        cma_channel = _get_context_slot( CTX_CMA_TX_ID );
    }

    // check NIC and CMA channels index
    if ( nic_channel != channel )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_stop(): illegal NIC channel\n");
        return -1;
    }
    if ( cma_channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_stop(): illegal CMA channel\n");
        return -1;
    }

    // desactivates the NIC channel
    _nic_channel_stop( nic_channel, is_rx );

    // desactivates the CMA channel
    _cma_set_register( cma_channel, CHBUF_RUN , 0 );

    return 0;
}  // end _sys_nic_stop()

////////////////////////////////////////
int _sys_nic_clear( unsigned int is_rx,
                    unsigned int channel )
{
    unsigned int nic_channel;

    // get NIC channel 
    if ( is_rx )  nic_channel = _get_context_slot( CTX_NIC_RX_ID );
    else          nic_channel = _get_context_slot( CTX_NIC_TX_ID );

    if ( nic_channel != channel )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_clear(): illegal NIC channel\n");
        return -1;
    }

    if ( is_rx )
    {
        _nic_set_global_register( NIC_G_NPKT_RX_G2S_RECEIVED       , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DES_TOO_SMALL      , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DES_TOO_BIG        , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DES_MFIFO_FULL     , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DES_CRC_FAIL       , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DISPATCH_RECEIVED  , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DISPATCH_BROADCAST , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DISPATCH_DST_FAIL  , 0 );
        _nic_set_global_register( NIC_G_NPKT_RX_DISPATCH_CH_FULL   , 0 );
    } 
    else
    {
        _nic_set_global_register( NIC_G_NPKT_TX_DISPATCH_RECEIVED  , 0 );
        _nic_set_global_register( NIC_G_NPKT_TX_DISPATCH_TRANSMIT  , 0 );
        _nic_set_global_register( NIC_G_NPKT_TX_DISPATCH_TOO_BIG   , 0 );
        _nic_set_global_register( NIC_G_NPKT_TX_DISPATCH_TOO_SMALL , 0 );
        _nic_set_global_register( NIC_G_NPKT_TX_DISPATCH_SRC_FAIL  , 0 );
        _nic_set_global_register( NIC_G_NPKT_TX_DISPATCH_BYPASS    , 0 );
        _nic_set_global_register( NIC_G_NPKT_TX_DISPATCH_BROADCAST , 0 );
    }
    return 0;
}  // en _sys_nic_clear()

////////////////////////////////////////
int _sys_nic_stats( unsigned int is_rx,
                    unsigned int channel )
{
    unsigned int nic_channel;

    // get NIC channel 
    if ( is_rx )  nic_channel = _get_context_slot( CTX_NIC_RX_ID );
    else          nic_channel = _get_context_slot( CTX_NIC_TX_ID );

    if ( nic_channel != channel )
    {
        _printf("\n[GIET_ERROR] in _sys_nic_stats(): illegal NIC channel\n");
        return -1;
    }

    if ( is_rx )
    {
        unsigned int received   = _nic_get_global_register( NIC_G_NPKT_RX_G2S_RECEIVED       );
        unsigned int too_small  = _nic_get_global_register( NIC_G_NPKT_RX_DES_TOO_SMALL      );
        unsigned int too_big    = _nic_get_global_register( NIC_G_NPKT_RX_DES_TOO_BIG        );
        unsigned int fifo_full  = _nic_get_global_register( NIC_G_NPKT_RX_DES_MFIFO_FULL     );
        unsigned int crc_fail   = _nic_get_global_register( NIC_G_NPKT_RX_DES_CRC_FAIL       );
        unsigned int broadcast  = _nic_get_global_register( NIC_G_NPKT_RX_DISPATCH_BROADCAST );
        unsigned int dst_fail   = _nic_get_global_register( NIC_G_NPKT_RX_DISPATCH_DST_FAIL  );
        unsigned int ch_full    = _nic_get_global_register( NIC_G_NPKT_RX_DISPATCH_CH_FULL   );

        _printf("\n### Network Controller RX Statistics ###\n"
                "- packets received : %d\n"
                "- too small        : %d\n"
                "- too big          : %d\n"
                "- fifo full        : %d\n" 
                "- crc fail         : %d\n" 
                "- dst mac fail     : %d\n" 
                "- channel full     : %d\n" 
                "- broadcast        : %d\n",
                received,
                too_small,
                too_big,
                fifo_full,
                crc_fail,
                dst_fail,
                ch_full,
                broadcast );
    } 
    else
    {
        unsigned int received   = _nic_get_global_register( NIC_G_NPKT_TX_DISPATCH_RECEIVED  );
        unsigned int too_big    = _nic_get_global_register( NIC_G_NPKT_TX_DISPATCH_TOO_BIG   );
        unsigned int too_small  = _nic_get_global_register( NIC_G_NPKT_TX_DISPATCH_TOO_SMALL );
        unsigned int src_fail   = _nic_get_global_register( NIC_G_NPKT_TX_DISPATCH_SRC_FAIL  );
        unsigned int bypass     = _nic_get_global_register( NIC_G_NPKT_TX_DISPATCH_BYPASS    );
        unsigned int broadcast  = _nic_get_global_register( NIC_G_NPKT_TX_DISPATCH_BROADCAST );

        _printf("\n### Network Controller TX Statistics ###\n"
                "- packets received : %d\n"
                "- too small        : %d\n"
                "- too big          : %d\n"
                "- src mac fail     : %d\n" 
                "- bypass           : %d\n" 
                "- broadcast        : %d\n",
                received,
                too_big,
                too_small,
                src_fail,
                bypass,
                broadcast );
    }
    return 0;
}  // end _sys_nic_stats()

/////////////////////////////////////////////////////////////////////////////////////////
//    FBF related syscall handlers
/////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////
int _sys_fbf_sync_write( unsigned int offset,
                         void*        buffer,
                         unsigned int length )
{
    char* fbf_address = (char *)SEG_FBF_BASE + offset;
    memcpy( fbf_address, buffer, length);

    return 0;
}

/////////////////////////////////////////////
int _sys_fbf_sync_read(  unsigned int offset,
                         void*        buffer,
                         unsigned int length )
{
    char* fbf_address = (char *)SEG_FBF_BASE + offset;
    memcpy( buffer, fbf_address, length);

    return 0;
}

////////////////////////
int _sys_fbf_cma_alloc()
{
    if ( _get_context_slot( CTX_CMA_FB_ID ) < NB_CMA_CHANNELS )
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_alloc() : CMA channel already allocated\n");
        return 0;
    }

    // get a new CMA channel index
    unsigned int channel;

    for ( channel = 0 ; channel < NB_CMA_CHANNELS ; channel++ )
    {
        if ( !_cma_channel[channel] )
        {
            _cma_channel[channel] = 1;
            break;
        }
    }

    if ( channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_alloc() : no CMA channel available\n");
        return -1;
    }
    else
    {
        _set_context_slot( CTX_CMA_FB_ID, channel );
        return 0;
    }
} // end sys_fbf_cma_alloc()

////////////////////////
// NOTE: not a syscall
int _sys_fbf_cma_release()
{
    unsigned int channel = _get_context_slot( CTX_CMA_FB_ID );

    if ( channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET_ERROR] in _sys_fbf_cma_release() : CMA channel already released\n");
        return -1;
    }

    // stop fb
    _sys_fbf_cma_stop();

    // reset CTX_CMA_FB_ID for task
    _set_context_slot( CTX_CMA_FB_ID, -1 );

    // release CMA channel
    _cma_channel[channel] = 0;

    return 0;
}

///////////////////////////////////////////////////
int _sys_fbf_cma_init_buf( void*        buf0_vbase, 
                           void*        buf1_vbase, 
                           void*        sts0_vaddr,
                           void*        sts1_vaddr )
{
#if NB_CMA_CHANNELS > 0

    unsigned int       vaddr;           // virtual address
    unsigned int       flags;           // for _v2p_translate()
    unsigned long long fbf_paddr;       // fbf physical address
    unsigned long long fbf_sts_paddr;   // fbf status physical address
    unsigned long long buf0_pbase;      // buffer 0 base physical address
    unsigned long long sts0_paddr;      // buffer 0 status physical address
    unsigned long long buf1_pbase;      // buffer 1 base physical address
    unsigned long long sts1_paddr;      // buffer 1 status physical address

    // get channel index
    unsigned int channel = _get_context_slot( CTX_CMA_FB_ID );

    if ( channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : CMA channel index too large\n");
        return -1;
    }

#if GIET_DEBUG_FBF_CMA
_printf("\n[FBF_CMA DEBUG] enters _sys_fbf_cma_init_buf()\n"
        " - channel           = %d\n"
        " - buf0        vbase = %x\n"
        " - buf1        vbase = %x\n"
        " - sts0        vaddr = %x\n"
        " - sts1        vaddr = %x\n",
        channel,
        (unsigned int)buf0_vbase,
        (unsigned int)buf1_vbase,
        (unsigned int)sts0_vaddr,
        (unsigned int)sts1_vaddr );
#endif

    // checking user buffers virtual addresses alignment
    if ( ((unsigned int)buf0_vbase & 0x3F) || 
         ((unsigned int)buf1_vbase & 0x3F) ) 
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : user buffer not aligned\n");
        return -1;
    }

    // checking user buffers status virtual addresses alignment
    if ( ((unsigned int)sts0_vaddr & 0x3F) ||
         ((unsigned int)sts1_vaddr & 0x3F) )
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : user status not aligned\n");
        return -1;
    }

    // compute frame buffer physical address and initialize _fbf_chbuf[channel]
    vaddr = (unsigned int)SEG_FBF_BASE;
    fbf_paddr = _v2p_translate( vaddr , &flags );
    vaddr = (unsigned int)&_fbf_status[channel];
    fbf_sts_paddr = _v2p_translate( vaddr , &flags );

    _fbf_chbuf[channel].fbf_desc =
        (unsigned long long) ((fbf_sts_paddr & 0xFFFFFFFFULL) >> 6) +
        (((fbf_paddr & 0xFFFFFFFFULL) >> 6 ) << 26);

    // Compute user buffer 0 physical addresses and intialize _fbf_chbuf[channel]
    vaddr = (unsigned int)buf0_vbase;
    buf0_pbase = _v2p_translate( vaddr , &flags );
    if ((flags & PTE_U) == 0) 
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : buf0 not in user space\n");
        return -1;
    }

    vaddr = (unsigned int)sts0_vaddr;
    sts0_paddr = _v2p_translate( vaddr , &flags );
    if ((flags & PTE_U) == 0) 
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : sts0 not in user space\n");
        return -1;
    }

    _fbf_chbuf[channel].buf0_desc = 
        (unsigned long long) ((sts0_paddr & 0xFFFFFFFFULL) >> 6) +
        (((buf0_pbase & 0xFFFFFFFFULL) >> 6 ) << 26);

    // Compute user buffer 1 physical addresses and intialize _fbf_chbuf[channel]
    vaddr = (unsigned int)buf1_vbase;
    buf1_pbase = _v2p_translate( vaddr , &flags );
    if ((flags & PTE_U) == 0) 
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : buf1 not in user space\n");
        return -1;
    }

    vaddr = (unsigned int)sts1_vaddr;
    sts1_paddr = _v2p_translate( vaddr , &flags );
    if ((flags & PTE_U) == 0) 
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : sts1 not in user space\n");
        return -1;
    }

    _fbf_chbuf[channel].buf1_desc = 
        (unsigned long long) ((sts1_paddr & 0xFFFFFFFFULL) >> 6) +
        (((buf1_pbase & 0xFFFFFFFFULL) >> 6 ) << 26);

    // Compute and register physical adress of the fbf_chbuf descriptor
    vaddr = (unsigned int)&_fbf_chbuf[channel];
    _fbf_chbuf_paddr[channel] = _v2p_translate( vaddr , &flags );
 
#if GIET_DEBUG_FBF_CMA
_printf(" - fbf         pbase = %l\n"
        " - fbf status  paddr = %l\n"
        " - buf0        pbase = %l\n"
        " - buf0 status paddr = %l\n" 
        " - buf1        pbase = %l\n"
        " - buf0 status paddr = %l\n" 
        " - chbuf       pbase = %l\n",
        fbf_paddr,
        fbf_sts_paddr,
        buf0_pbase,
        sts0_paddr,
        buf1_pbase,
        sts1_paddr,
        _fbf_chbuf_paddr[channel] );
#endif

    return 0;

#else

    _printf("\n[GIET ERROR] in _sys_fbf_cma_init_buf() : NB_CMA_CHANNELS = 0\n");
    return -1;

#endif  
} // end sys_fbf_cma_init_buf()

////////////////////////////////////////////
int _sys_fbf_cma_start( unsigned int length ) 
{
#if NB_CMA_CHANNELS > 0

    // get channel index
    unsigned int channel = _get_context_slot( CTX_CMA_FB_ID );

    if ( channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET ERROR] in _fbf_cma_start() : CMA channel index too large\n");
        return -1;
    }

    // check buffers initialization
    if ( ( _fbf_chbuf[channel].buf0_desc == 0x0ULL ) &&
         ( _fbf_chbuf[channel].buf1_desc == 0x0ULL) &&
         ( _fbf_chbuf[channel].fbf_desc == 0x0ULL) )
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_start() :\n"
                "Buffer initialization has not been done\n");
        return -1;
    }

    // initializes buffer length
    _fbf_chbuf[channel].length = length;

    if ( USE_IOB )
    {
        // SYNC request for fbf_chbuf descriptor
        _mmc_sync( _fbf_chbuf_paddr[channel] , sizeof( fbf_chbuf_t ) );
    }

    // start CMA transfer
    unsigned long long paddr = _fbf_chbuf_paddr[channel];
    unsigned int src_chbuf_paddr_lsb = (unsigned int)(paddr & 0xFFFFFFFF);
    unsigned int src_chbuf_paddr_ext = (unsigned int)(paddr >> 32);
    unsigned int dst_chbuf_paddr_lsb = src_chbuf_paddr_lsb + 16;
    unsigned int dst_chbuf_paddr_ext = src_chbuf_paddr_ext;

    _cma_set_register( channel, CHBUF_SRC_DESC , src_chbuf_paddr_lsb );
    _cma_set_register( channel, CHBUF_SRC_EXT  , src_chbuf_paddr_ext );
    _cma_set_register( channel, CHBUF_SRC_NBUFS, 2 );
    _cma_set_register( channel, CHBUF_DST_DESC , dst_chbuf_paddr_lsb );
    _cma_set_register( channel, CHBUF_DST_EXT  , dst_chbuf_paddr_ext );
    _cma_set_register( channel, CHBUF_DST_NBUFS, 1 );
    _cma_set_register( channel, CHBUF_BUF_SIZE , length );
    _cma_set_register( channel, CHBUF_PERIOD   , 300 );
    _cma_set_register( channel, CHBUF_RUN      , 1 );

    return 0;

#else

    _printf("\n[GIET ERROR] in _sys_fbf_cma_start() : NB_CMA_CHANNELS = 0\n");
    return -1;

#endif
} // end _sys_fbf_cma_start()

/////////////////////////////////////////////////////
int _sys_fbf_cma_display( unsigned int buffer_index )
{
#if NB_CMA_CHANNELS > 0

    volatile unsigned int full = 1;

    // get channel index
    unsigned int channel = _get_context_slot( CTX_CMA_FB_ID );

    if ( channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_display() : "
                "CMA channel index too large\n");
        return -1;
    }

    // get fbf_chbuf descriptor pointer
    fbf_chbuf_t* pdesc = &_fbf_chbuf[channel];     

#if GIET_DEBUG_FBF_CMA
_printf("\n[FBF_CMA DEBUG] enters _sys_fb_cma_display()\n"
        " - cma channel      = %d\n"
        " - buffer index     = %d\n"
        " - buf0_desc value  = %l\n"
        " - buf1_desc value  = %l\n"
        " - fbf_desc  value  = %l\n",
        channel , buffer_index,
        _fbf_chbuf[channel].buf0_desc,
        _fbf_chbuf[channel].buf1_desc,
        _fbf_chbuf[channel].fbf_desc );
#endif

    unsigned long long buf_sts_paddr;
    unsigned long long buf_paddr;
    unsigned long long fbf_sts_paddr;

    if ( buffer_index == 0 )    // user buffer 0
    {
        buf_sts_paddr =
            ((pdesc->buf0_desc & 0xFFF0000000000000ULL) >> 20) + // compute address extension
            ((pdesc->buf0_desc & 0x3FFFFFFULL) << 6);            // compute 32 LSB of the address

        buf_paddr =
            (pdesc->buf0_desc & 0xFFFFFFFFFC000000ULL) >> 20;  // compute the entire address
    }
    else                        // user buffer 1
    {
        buf_sts_paddr =
            ((pdesc->buf1_desc & 0xFFF0000000000000ULL) >> 20) +
            ((pdesc->buf1_desc & 0x3FFFFFFULL) << 6);

        buf_paddr =
            (pdesc->buf1_desc & 0xFFFFFFFFFC000000ULL) >> 20;
    }

    fbf_sts_paddr =
        ((pdesc->fbf_desc & 0xFFF0000000000000ULL) >> 20) +
        ((pdesc->fbf_desc & 0x3FFFFFFULL) << 6);

#if GIET_DEBUG_FBF_CMA
_printf(" - fbf status paddr = %l\n"
        " - buf        pbase = %l\n"
        " - buf status paddr = %l\n",
        fbf_sts_paddr,
        buf_paddr,
        buf_sts_paddr );
#endif
        
    // waiting user buffer released by the CMA component)
    while ( full )
    {  
        // INVAL L2 cache copy of user buffer status     
        // because it has been modified in RAM by the CMA component 
        _mmc_inval( buf_sts_paddr , 4 );        

        full = _physical_read( buf_sts_paddr );
    }

    // SYNC request for the user buffer, because 
    // it will be read from XRAM by the CMA component
    _mmc_sync( buf_paddr , pdesc->length );

    // set user buffer status
    _physical_write( buf_sts_paddr, 0x1 );

    // reset fbf buffer status
    _physical_write( fbf_sts_paddr, 0x0 );
    
    // SYNC request, because these buffer descriptors
    // will be read from XRAM by the CMA component
    _mmc_sync( buf_sts_paddr, 4 );
    _mmc_sync( fbf_sts_paddr, 4 );

    return 0;

#else

    _printf("\n[GIET ERROR] in _sys_fbf_cma_display() : no CMA channel allocated\n");
    return -1;

#endif
} // end _sys_fbf_cma_display()

///////////////////////
int _sys_fbf_cma_stop()
{
#if NB_CMA_CHANNELS > 0

    // get channel index
    unsigned int channel = _get_context_slot( CTX_CMA_FB_ID );

    if ( channel >= NB_CMA_CHANNELS )
    {
        _printf("\n[GIET ERROR] in _sys_fbf_cma_stop() : CMA channel index too large\n");
        return -1;
    }

    // Desactivate CMA channel
    _cma_set_register( channel, CHBUF_RUN, 0 );

    return 0;

#else

    _printf("\n[GIET ERROR] in _sys_fbf_cma_stop() : no CMA channel allocated\n");
    return -1;

#endif
} // end _sys_fbf_cma_stop()


//////////////////////////////////////////////////////////////////////////////
//           Miscelaneous syscall handlers 
//////////////////////////////////////////////////////////////////////////////

///////////////
int _sys_ukn() 
{
    _printf("\n[GIET ERROR] Undefined System Call / EPC = %x\n", _get_epc() );
    return -1;
}

////////////////////////////////////
int _sys_proc_xyp( unsigned int* x,
                   unsigned int* y,
                   unsigned int* p )
{
    unsigned int gpid = _get_procid();  // global processor index from CPO register

    *x = (gpid >> (Y_WIDTH + P_WIDTH)) & ((1<<X_WIDTH)-1);
    *y = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    *p = gpid & ((1<<P_WIDTH)-1);

    return 0;
}

//////////////////////////////////
int _sys_task_exit( char* string ) 
{
    unsigned int date       = _get_proctime();

    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int p          = gpid & ((1<<P_WIDTH)-1);

    unsigned int ltid       = _get_context_slot(CTX_LTID_ID);

    // print exit message
    _printf("\n[GIET] Exit task %d on processor[%d,%d,%d] at cycle %d"
            "\n       Cause : %s\n\n",
            ltid, x, y, p, date, string );

    // set NORUN_MASK_TASK bit (non runnable state)
    static_scheduler_t*  psched  = (static_scheduler_t*)_schedulers[x][y][p];
    unsigned int*        ptr     = &psched->context[ltid][CTX_NORUN_ID];
    _atomic_or( ptr , NORUN_MASK_TASK );

    // deschedule
    _sys_context_switch();

    return 0;
} 

/////////////////////////
int _sys_context_switch() 
{
    unsigned int save_sr;

    _it_disable( &save_sr );
    _ctx_switch();
    _it_restore( &save_sr );

    return 0;
}

////////////////////////
int _sys_local_task_id()
{
    return _get_context_slot(CTX_LTID_ID);
}

/////////////////////////
int _sys_global_task_id()
{
    return _get_context_slot(CTX_GTID_ID);
}

////////////////////
int _sys_thread_id()
{
    return _get_context_slot(CTX_TRDID_ID);
}

////////////////////////////////////////////
int _sys_procs_number( unsigned int* x_size,
                       unsigned int* y_size,
                       unsigned int* nprocs )
{
    mapping_header_t * header   = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t * cluster = _get_cluster_base(header);

    unsigned int x;
    unsigned int y;
    unsigned int okmin = 1;
    unsigned int okmax = 1;

    // compute max values
    unsigned int xmax  = header->x_size;
    unsigned int ymax  = header->y_size;
    unsigned int procs = cluster[0].procs;

    // check the (ymax-1) lower rows
    for ( y = 0 ; y < ymax-1 ; y++ )
    {
        for ( x = 0 ; x < xmax ; x++ )
        {
            if (cluster[x*ymax+y].procs != procs ) okmin = 0;
        }
    }

    // check the upper row
    for ( x = 0 ; x < xmax ; x++ )
    {
        if (cluster[x*ymax+ymax-1].procs != procs ) okmax = 0;
    }

    // return values
    if ( okmin && okmax )
    {
        *x_size = xmax;
        *y_size = ymax;
        *nprocs = procs;
    }
    else if ( okmin )
    {
        *x_size = xmax;
        *y_size = ymax-1;
        *nprocs = procs;
    }
    else
    {
        *x_size = 0;
        *y_size = 0;
        *nprocs = 0;
    }
    return 0;
}

///////////////////////////////////////////////////////
int _sys_vseg_get_vbase( char*             vspace_name, 
                         char*             vseg_name, 
                         unsigned int*     vbase ) 
{
    mapping_header_t * header = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t * vspace = _get_vspace_base(header);
    mapping_vseg_t * vseg     = _get_vseg_base(header);

    unsigned int vspace_id;
    unsigned int vseg_id;

    // scan vspaces 
    for (vspace_id = 0; vspace_id < header->vspaces; vspace_id++) 
    {
        if (_strncmp( vspace[vspace_id].name, vspace_name, 31) == 0) 
        {
            // scan vsegs
            for (vseg_id = vspace[vspace_id].vseg_offset; 
                 vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs); 
                 vseg_id++) 
            {
                if (_strncmp(vseg[vseg_id].name, vseg_name, 31) == 0) 
                {
                    *vbase = vseg[vseg_id].vbase;
                    return 0;
                }
            } 
        }
    } 
    return -1;    // not found 
}

/////////////////////////////////////////////////////////
int _sys_vseg_get_length( char*         vspace_name, 
                          char*         vseg_name,
                          unsigned int* length ) 
{
    mapping_header_t * header = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t * vspace = _get_vspace_base(header);
    mapping_vseg_t * vseg     = _get_vseg_base(header);

    unsigned int vspace_id;
    unsigned int vseg_id;

    // scan vspaces 
    for (vspace_id = 0; vspace_id < header->vspaces; vspace_id++) 
    {
        if (_strncmp( vspace[vspace_id].name, vspace_name, 31) == 0) 
        {
            // scan vsegs
            for (vseg_id = vspace[vspace_id].vseg_offset; 
                 vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs); 
                 vseg_id++) 
            {
                if (_strncmp(vseg[vseg_id].name, vseg_name, 31) == 0) 
                {
                    *length = vseg[vseg_id].length;
                    return 0;
                }
            } 
        }
    } 
    return -1;    // not found 
}

////////////////////////////////////////
int _sys_xy_from_ptr( void*         ptr,
                      unsigned int* x,
                      unsigned int* y )
{
    unsigned int flags;
    unsigned long long paddr = _v2p_translate( (unsigned int)ptr , &flags );
    
    *x = (paddr>>36) & 0xF;
    *y = (paddr>>32) & 0xF;

    return 0;
}

/////////////////////////////////////////
int _sys_heap_info( unsigned int* vaddr, 
                    unsigned int* length,
                    unsigned int  x,
                    unsigned int  y ) 
{
    mapping_header_t * header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_task_t *   task    = _get_task_base(header);
    mapping_vseg_t *   vseg    = _get_vseg_base(header);
    mapping_vspace_t * vspace  = _get_vspace_base(header);

    unsigned int task_id;
    unsigned int vspace_id;
    unsigned int vseg_id = 0xFFFFFFFF;

    // searching the heap vseg
    if ( (x < X_SIZE) && (y < Y_SIZE) )  // searching a task in cluster(x,y)
    {
        // get vspace global index
        vspace_id = _get_context_slot(CTX_VSID_ID);

        // scan all tasks in vspace
        unsigned int min = vspace[vspace_id].task_offset ;
        unsigned int max = min + vspace[vspace_id].tasks ;
        for ( task_id = min ; task_id < max ; task_id++ )
        {
            if ( task[task_id].clusterid == (x * Y_SIZE + y) )
            {
                vseg_id = task[task_id].heap_vseg_id;
                if ( vseg_id != 0xFFFFFFFF ) break;
            }
        }
    }
    else                                // searching in the calling task 
    {
        task_id = _get_context_slot(CTX_GTID_ID);
        vseg_id = task[task_id].heap_vseg_id;
    }

    // analysing the vseg_id
    if ( vseg_id != 0xFFFFFFFF ) 
    {
        *vaddr  = vseg[vseg_id].vbase;
        *length = vseg[vseg_id].length;
        return 0;
    }
    else 
    {
        *vaddr  = 0;
        *length = 0;
        return -1;
    }
}  // end _sys_heap_info()


///////////////////////
int _sys_tasks_status()
{
    mapping_header_t *  header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_task_t *    task    = _get_task_base(header);
    mapping_vspace_t *  vspace  = _get_vspace_base(header);
    mapping_cluster_t * cluster = _get_cluster_base(header);

    unsigned int task_id;
    unsigned int vspace_id;

    // scan all vspaces
    for( vspace_id = 0 ; vspace_id < header->vspaces ; vspace_id++ )
    {
        _printf("\n*** vspace %s\n", vspace[vspace_id].name );

        // scan all tasks in vspace
        unsigned int min = vspace[vspace_id].task_offset ;
        unsigned int max = min + vspace[vspace_id].tasks ;
        for ( task_id = min ; task_id < max ; task_id++ )
        {
            unsigned int         clusterid = task[task_id].clusterid;
            unsigned int         p         = task[task_id].proclocid;
            unsigned int         x         = cluster[clusterid].x;
            unsigned int         y         = cluster[clusterid].y;
            unsigned int         ltid      = task[task_id].ltid;
            static_scheduler_t*  psched    = (static_scheduler_t*)_schedulers[x][y][p];
            unsigned int         norun     = psched->context[ltid][CTX_NORUN_ID];
            unsigned int         current   = psched->current;

            if ( current == ltid )
            _printf(" - task %s on P[%d,%d,%d] : running\n", task[task_id].name, x, y, p );
            else if ( norun == 0 )
            _printf(" - task %s on P[%d,%d,%d] : runable\n", task[task_id].name, x, y, p );
            else
            _printf(" - task %s on P[%d,%d,%d] : blocked\n", task[task_id].name, x, y, p );
        }
    }
    return 0;
}  // end _sys_tasks_status()



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

