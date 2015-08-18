///////////////////////////////////////////////////////////////////////////
// File     : irq_handler.c
// Date     : 01/04/2012
// Author   : alain greiner 
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <irq_handler.h>
#include <sys_handler.h>
#include <ctx_handler.h>
#include <tim_driver.h>
#include <xcu_driver.h>
#include <pic_driver.h>
#include <tty_driver.h>
#include <nic_driver.h>
#include <cma_driver.h>
#include <mmc_driver.h>
#include <bdv_driver.h>
#include <hba_driver.h>
#include <dma_driver.h>
#include <sdc_driver.h>
#include <mwr_driver.h>
#include <mapping_info.h>
#include <utils.h>
#include <tty0.h>

/////////////////////////////////////////////////////////////////////////
//       Global variables 
/////////////////////////////////////////////////////////////////////////

// array of external IRQ indexes for each (isr/channel) couple 
__attribute__((section(".kdata")))
unsigned char _ext_irq_index[GIET_ISR_TYPE_MAX][GIET_ISR_CHANNEL_MAX];

// WTI mailbox allocators for external IRQ routing (3 allocators per proc)
__attribute__((section(".kdata")))
unsigned char _wti_alloc_one[X_SIZE][Y_SIZE][NB_PROCS_MAX];
__attribute__((section(".kdata")))
unsigned char _wti_alloc_two[X_SIZE][Y_SIZE][NB_PROCS_MAX];
__attribute__((section(".kdata")))
unsigned char _wti_alloc_ter[X_SIZE][Y_SIZE][NB_PROCS_MAX];

/////////////////////////////////////////////////////////////////////////
// this array is allocated in the boot.c or kernel_init.c
/////////////////////////////////////////////////////////////////////////

extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX]; 

/////////////////////////////////////////////////////////////////////////
// These ISR_TYPE names for display must be consistent with values in
// irq_handler.h / mapping.py / xml_driver.c
/////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
char* _isr_type_str[] = { "DEFAULT",
                          "TICK"   ,
                          "TTY_RX" ,
                          "TTY_TX" ,
                          "BDV"    ,
                          "TIMER"  ,
                          "WAKUP"  ,
                          "NIC_RX" ,
                          "NIC_TX" ,
                          "CMA"    ,
                          "MMC"    ,
                          "DMA"    ,
                          "SPI"    ,
                          "MWR"    ,
                          "HBA"    };

__attribute__((section(".kdata")))
char* _irq_type_str[] = { "HWI", 
                          "WTI", 
                          "PTI" }; 

////////////////////
void _ext_irq_init()
{
    mapping_header_t*    header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t*   cluster = _get_cluster_base(header);
    mapping_periph_t*    periph  = _get_periph_base(header);
    mapping_irq_t*       irq     = _get_irq_base(header);
    unsigned int         periph_id;   // peripheral index in mapping_info
    unsigned int         irq_id;      // irq index in mapping_info

    // get cluster_io index in mapping
    unsigned int         x_io       = header->x_io;  
    unsigned int         y_io       = header->y_io;  
    unsigned int         cluster_io = (x_io * Y_SIZE) + y_io; 
    mapping_periph_t*    pic        = NULL;

    // scan external peripherals to find PIC
    unsigned int min = cluster[cluster_io].periph_offset ;
    unsigned int max = min + cluster[cluster_io].periphs ;
    
    for ( periph_id = min ; periph_id < max ; periph_id++ )
    {
        if ( periph[periph_id].type == PERIPH_TYPE_PIC ) 
        {
            pic = &periph[periph_id];
            break;
        }
    }  

    if ( pic == NULL )
    {
        _printf("\n[GIET ERROR] in _ext_irq_init() : No PIC component found\n");
        _exit();
    }

    // scan PIC IRQS defined in mapping
    for ( irq_id = pic->irq_offset ;
          irq_id < pic->irq_offset + pic->irqs ;
          irq_id++ )
    {
        unsigned int type    = irq[irq_id].srctype;
        unsigned int srcid   = irq[irq_id].srcid;
        unsigned int isr     = irq[irq_id].isr;
        unsigned int channel = irq[irq_id].channel;

        if ( (type != IRQ_TYPE_HWI)            || 
             (srcid > 31)                      || 
             (isr >= GIET_ISR_TYPE_MAX)        ||
             (channel >= GIET_ISR_CHANNEL_MAX) )
        {
            _printf("\n[GIET ERROR] in _ext_irq_init() : Bad PIC IRQ\n"
                    "  type = %d / srcid = %d / isr = %d / channel = %d\n",
                    type , srcid , isr , channel );
            _exit();
        }
        _ext_irq_index[isr][channel] = srcid;
    }
}  // end _ext_irq_init()

////////////////////////////////////////////
void _ext_irq_alloc( unsigned int   isr_type,
                     unsigned int   isr_channel,
                     unsigned int*  wti_index )
{
    unsigned int wti_id;        // allocated WTI mailbox index in XCU
    unsigned int irq_id;        // external IRQ index in PIC (input)
    unsigned int wti_addr;      // WTI mailbox physical address (32 lsb bits)

    // check arguments
    if ( isr_type >= GIET_ISR_TYPE_MAX )
    {
        _printf("\n[GIET ERROR] in _ext_irq_alloc() : illegal ISR type\n");
        _exit();
    }
    if ( isr_channel >= GIET_ISR_CHANNEL_MAX )
    {
        _printf("\n[GIET ERROR] in _ext_irq_alloc() : illegal ISR channel\n");
        _exit();
    }

    // get processor coordinates [x,y,p]
    unsigned int gpid           = _get_procid();
    unsigned int cluster_xy     = gpid >> P_WIDTH;
    unsigned int x              = cluster_xy >> Y_WIDTH;
    unsigned int y              = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p              = gpid & ((1<<P_WIDTH)-1);

    // allocate a WTI mailbox to proc[x,y,p] (blocking until success)
    while ( 1 )
    {
        if ( _wti_alloc_one[x][y][p] == 0 )
        {
            _wti_alloc_one[x][y][p] = 1;
            wti_id = p + NB_PROCS_MAX;
            break;
        }
        if ( _wti_alloc_two[x][y][p] == 0 )
        {
            _wti_alloc_two[x][y][p] = 1;
            wti_id = p + 2*NB_PROCS_MAX;
            break;
        }
        if ( _wti_alloc_ter[x][y][p] == 0 )
        {
            _wti_alloc_ter[x][y][p] = 1;
            wti_id = p + 3*NB_PROCS_MAX;
            break;
        }
    }    
    *wti_index = wti_id;

    // register the mailbox physical address in IOPIC
    irq_id   = _ext_irq_index[isr_type][isr_channel];
    _xcu_get_wti_address( wti_id , &wti_addr );
    _pic_init( irq_id , wti_addr, cluster_xy );
    
    // initializes the WTI interrupt vector entry for target XCU
    static_scheduler_t*  psched = (static_scheduler_t*)_get_sched();
    psched->wti_vector[wti_id] = isr_channel<<16 | isr_type;

#if GIET_DEBUG_IRQS
_printf("\n[DEBUG IRQS] _ext_irq_alloc() for P[%d,%d,%d] at cycle %d\n"
        "  wti_id = %d / isr_type = %s / channel = %d / pic_input = %d\n",
        x , y , p , _get_proctime() ,
        wti_id , _isr_type_str[isr_type] , isr_channel , irq_id );
#endif

}  // end ext_irq_alloc()

////////////////////////////////////////////
void _ext_irq_release( unsigned int isr_type,
                       unsigned int isr_channel )
{
    unsigned int wti_id;        // allocated WTI mailbox index in XCU
    unsigned int irq_id;        // external IRQ index in PIC (input)

    // get processor coordinates [x,y,p]
    unsigned int gpid           = _get_procid();
    unsigned int cluster_xy     = gpid >> P_WIDTH;
    unsigned int x              = cluster_xy >> Y_WIDTH;
    unsigned int y              = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p              = gpid & ((1<<P_WIDTH)-1);

    // check arguments
    if ( isr_type >= GIET_ISR_TYPE_MAX )
    {
        _printf("\n[GIET ERROR] in _ext_irq_release() illegal ISR type\n");
        _exit();
    }
    if ( isr_channel >= GIET_ISR_CHANNEL_MAX )
    {
        _printf("\n[GIET ERROR] in _ext_irq_release() : illegal ISR channel\n");
        _exit();
    }

    // find WTI index
    static_scheduler_t*  psched = (static_scheduler_t*)_get_sched();
    for ( wti_id = 0 ; wti_id < 32 ; wti_id++ )
    {
        if ( psched->wti_vector[wti_id] == (isr_channel<<16 | isr_type) )
            break;
    }
    if ( wti_id == 32 )
    {
        _printf("\n[GIET ERROR] in _ext_irq_release() : isr not found\n");
        return;
    }

    // desactivates dynamically allocated PIC entry
    irq_id = _ext_irq_index[isr_type][isr_channel];
    _pic_set_register( irq_id , IOPIC_MASK , 0 );

    // releases dynamically allocated WTI mailbox
    if      ( wti_id == p +   NB_PROCS_MAX ) _wti_alloc_one[x][y][p] = 0;
    else if ( wti_id == p + 2*NB_PROCS_MAX ) _wti_alloc_two[x][y][p] = 0;
    else if ( wti_id == p + 3*NB_PROCS_MAX ) _wti_alloc_ter[x][y][p] = 0;
    else
    {
        _printf("\n[GIET ERROR] in _ext_irq_release() : illegal WTI index\n");
        _exit();
    }
}  // end ext_irq_release()

/////////////////
void _irq_demux() 
{
    unsigned int gpid           = _get_procid();
    unsigned int cluster_xy     = gpid >> P_WIDTH;
    unsigned int x              = cluster_xy >> Y_WIDTH;
    unsigned int y              = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p              = gpid & ((1<<P_WIDTH)-1);
    unsigned int irq_id;
    unsigned int irq_type;

    // get the highest priority active IRQ index 
    unsigned int icu_out_index = p * IRQ_PER_PROCESSOR;

    _xcu_get_index( cluster_xy, icu_out_index, &irq_id, &irq_type );

    if (irq_id < 32) 
    {
        static_scheduler_t* psched = (static_scheduler_t*)_get_sched();
        unsigned int        entry = 0;
        unsigned int        isr_type;
        unsigned int        channel;

        if      (irq_type == IRQ_TYPE_HWI) entry = psched->hwi_vector[irq_id];
        else if (irq_type == IRQ_TYPE_PTI) entry = psched->pti_vector[irq_id];
        else if (irq_type == IRQ_TYPE_WTI) entry = psched->wti_vector[irq_id];
        else
        {
            _printf("\n[GIET ERROR] illegal irq_type in irq_demux()\n");
            _exit();
        }

        isr_type   = (entry    ) & 0x0000FFFF;
        channel    = (entry>>16) & 0x00007FFF;

#if GIET_DEBUG_IRQS    // we don't take the TTY lock to avoid deadlocks
_nolock_printf("\n[DEBUG IRQS] _irq_demux() Processor[%d,%d,%d] enters at cycle %d\n"
               " irq_type = %s / irq_id = %d / isr_type = %s / channel = %d\n",
               x , y , p , _get_proctime() ,
               _irq_type_str[irq_type] , irq_id , _isr_type_str[isr_type] , channel );   
#endif

        // ISR call
        if      ( isr_type == ISR_TICK   ) _isr_tick   ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_TTY_RX ) _tty_rx_isr ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_TTY_TX ) _tty_tx_isr ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_BDV    ) _bdv_isr    ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_TIMER  ) _timer_isr  ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_WAKUP  ) _isr_wakup  ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_NIC_RX ) _nic_rx_isr ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_NIC_TX ) _nic_tx_isr ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_CMA    ) _cma_isr    ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_MMC    ) _mmc_isr    ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_DMA    ) _dma_isr    ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_SDC    ) _sdc_isr    ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_MWR    ) _mwr_isr    ( irq_type, irq_id, channel );
        else if ( isr_type == ISR_HBA    ) _hba_isr    ( irq_type, irq_id, channel );
        else
        {
            _printf("\n[GIET ERROR] in _irq_demux() :"
                    " illegal ISR type on processor[%d,%d,%d] at cycle %d\n"
                    " - irq_type = %s\n"
                    " - irq_id   = %d\n"
                    " - isr_type = %s\n",
                    x, y, p, _get_proctime(), 
                    _irq_type_str[irq_type] , irq_id , _isr_type_str[isr_type] );   
            _exit();
        }
    }
    else   // no interrupt active
    {
        _isr_default();
    } 
}

///////////////////
void _isr_default()
{
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p          = gpid & ((1<<P_WIDTH)-1);

    _printf("\n[GIET WARNING] IRQ handler called but no active IRQ "
            "on processor[%d,%d,%d] at cycle %d\n",
            x, y, p, _get_proctime() );
}


////////////////////////////////////////////////////////////
void _isr_wakup( unsigned int irq_type,   // HWI / WTI / PTI
                 unsigned int irq_id,     // index returned by ICU
                 unsigned int channel )   // unused
{
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p          = gpid & ((1<<P_WIDTH)-1);

    unsigned int value;     // WTI mailbox value
    unsigned int save_sr;   // save SR value in pre-empted task stack

    unsigned int ltid       = _get_current_task_id();

    if ( irq_type != IRQ_TYPE_WTI )
    {
        _printf("[GIET ERROR] P[%d,%d,%d] enters _isr_wakup() at cycle %d\n"
                " but not called by a WTI interrupt\n",
                x , y , p , _get_proctime() );
        _exit();
    }

    // get mailbox value and acknowledge WTI
    _xcu_get_wti_value( cluster_xy, irq_id, &value );

#if GIET_DEBUG_SWITCH
_printf("\n[DEBUG SWITCH] P[%d,%d,%d] enters _isr_wakup() at cycle %d\n"
        "  WTI index = %d / current ltid = %d / mailbox value = %x\n",
        x , y , p , _get_proctime() , irq_id , ltid , value );
#endif

    // enter critical section and swich context (if required)
    if ( (ltid == IDLE_TASK_INDEX) || (value != 0) )
    {
        _it_disable( &save_sr );
        _ctx_switch();
        _it_restore( &save_sr );
    }

} // end _isr_wakup

///////////////////////////////////////////////////////////
void _isr_tick( unsigned int irq_type,   // HWI / WTI / PTI
                unsigned int irq_id,     // index returned by ICU
                unsigned int channel )   // channel index if HWI
{
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p          = gpid & ((1<<P_WIDTH)-1);

    unsigned int save_sr;   // save SR value in pre-empted task stack

    if ( irq_type != IRQ_TYPE_PTI )
    {
        _printf("[GIET ERROR] P[%d,%d,%d] enters _isr_tick() at cycle %d\n"
                " but not called by a PTI interrupt\n",
                x , y , p , _get_proctime() );
        _exit();
    }

    // acknowledge PTI
    _xcu_timer_reset_irq( cluster_xy, irq_id );

#if GIET_DEBUG_SWITCH
unsigned int ltid  = _get_current_task_id();
_printf("\n[DEBUG SWITCH] P[%d,%d,%d] enters _isr_tick() at cycle %d\n"
        "  WTI index = %d / current ltid = %d\n",
        x , y , p , _get_proctime() , irq_id , ltid );
#endif

    // enter critical section and switch context 
    _it_disable( &save_sr );
    _ctx_switch();
    _it_restore( &save_sr );

}  // end _isr_tick


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

