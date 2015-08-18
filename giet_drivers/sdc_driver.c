///////////////////////////////////////////////////////////////////////////////////
// File     : sdc_driver.c
// Date     : 31/04/2015
// Author   : Alain Greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include "hard_config.h"
#include "giet_config.h"
#include "sdc_driver.h"
#include "tty0.h"
#include "utils.h"
#include "vmem.h"
#include "kernel_locks.h"
#include "mmc_driver.h"
#include "xcu_driver.h"
#include "ctx_handler.h"

#define  SDC_RSP_TIMEOUT      100       // number of retries for a config RSP

#define  SDC_POLLING_TIMEOUT  1000000   // number of retries for polling PXCI 

//////////////////////////////////////////////////////////////////////////////////
//      Extern variables
//////////////////////////////////////////////////////////////////////////////////

// allocated in the boot.c or kernel_init.c files
extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX]; 

///////////////////////////////////////////////////////////////////////////////////
//          AHCI related global variables
///////////////////////////////////////////////////////////////////////////////////

// global index ot the task, for each entry in the command list
__attribute__((section(".kdata")))
unsigned int     _ahci_gtid[32];

// status of the command, for each entry in the command list
__attribute__((section(".kdata")))
unsigned int     _ahci_status[32];

// command list : up to 32 commands
__attribute__((section(".kdata")))
ahci_cmd_desc_t  _ahci_cmd_list[32] __attribute__((aligned(0x40)));   

// command tables array : one command table per entry in command list
__attribute__((section(".kdata")))
ahci_cmd_table_t _ahci_cmd_table[32] __attribute__((aligned(0x40))); 

// command list write index : next slot to register a command 
__attribute__((section(".kdata")))
unsigned int     _ahci_cmd_ptw;

// command list read index : next slot to poll a completed command 
__attribute__((section(".kdata")))
unsigned int     _ahci_cmd_ptr;


///////////////////////////////////////////////////////////////////////////////////
//          SD Card related global variables
///////////////////////////////////////////////////////////////////////////////////

// SD card relative address
__attribute__((section(".kdata")))
unsigned int     _sdc_rca;

// SD Card Hih Capacity Support when non zero
__attribute__((section(".kdata")))
unsigned int     _sdc_sdhc;


///////////////////////////////////////////////////////////////////////////////
// This low_level function returns the value contained in register (index).
///////////////////////////////////////////////////////////////////////////////
static unsigned int _sdc_get_register( unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_IOC_BASE + index;
    return _io_extended_read( vaddr );
}

///////////////////////////////////////////////////////////////////////////////
// This low-level function set a new value in register (index).
///////////////////////////////////////////////////////////////////////////////
static void _sdc_set_register( unsigned int index,
                               unsigned int value ) 
{
    unsigned int* vaddr = (unsigned int*)SEG_IOC_BASE + index;
    _io_extended_write( vaddr, value );
}

///////////////////////////////////////////////////////////////////////////////
// This function sends a command to the SD card and returns the response.
// - index      : CMD index
// - arg        : CMD argument
// - return Card response
///////////////////////////////////////////////////////////////////////////////
static unsigned int _sdc_send_cmd ( unsigned int   index,
                                    unsigned int   arg )
{
    unsigned int  sdc_rsp;
    register int  iter = 0;

    // load argument
    _sdc_set_register( SDC_CMD_ARG, arg );

    // lauch command
    _sdc_set_register( SDC_CMD_ID , index );

    // get response
    do
    {
        sdc_rsp = _sdc_get_register( SDC_RSP_STS );
        iter++;
    }
    while ( (sdc_rsp == 0xFFFFFFFF) && (iter < SDC_RSP_TIMEOUT) ); 

    return sdc_rsp;
}

/////////////////////////////////////////////////////////////////////////////////
//           Extern functions
/////////////////////////////////////////////////////////////////////////////////

////////////////////////
unsigned int _sdc_init()
{
    //////////// SD Card initialisation //////////
  
    unsigned int rsp;

    // define the SD card clock period
    _sdc_set_register( SDC_PERIOD , GIET_SDC_PERIOD );

    // send CMD0 (soft reset / no argument)
    rsp = _sdc_send_cmd( SDC_CMD0 , 0 );
    if ( rsp == 0xFFFFFFFF )
    {
        _printf("\n[SDC ERROR] in _sdc_init() : no acknowledge to CMD0\n");
        return 1;
    }

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_init() : SDC_CMD0 done at cycle %d\n", _get_proctime() );
#endif

    // send CMD8 command 
    rsp = _sdc_send_cmd( SDC_CMD8 , SDC_CMD8_ARGUMENT );
    if ( rsp == 0xFFFFFFFF )
    {
        _printf("\n[SDC ERROR] in _sdc_init() : no response to CMD8\n");
        return 1;
    }
    else if ( rsp != SDC_CMD8_ARGUMENT )
    {
        _printf("\n[SDC ERROR] in _sdc_init() : response to CMD8 = %x / expected = %x\n",
                rsp , SDC_CMD8_ARGUMENT );
        return 1;
    }

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_init() : SDC_CMD8 done at cycle %d\n", _get_proctime() );
#endif

    // send CMD41 to get SDHC
    if ( rsp == 0xFFFFFFFF )
    {
        _printf("\n[SDC ERROR] in _sdc_init() : no response to CMD41\n");
        return 1;
    }
    _sdc_sdhc = ( (rsp & SDC_CMD41_RSP_CCS) != 0 );

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_init() : SDC_CMD41 done at cycle %d\n", _get_proctime() );
#endif

    // send CMD3 to get RCA
    rsp = _sdc_send_cmd( SDC_CMD3 , 0 );
    if ( rsp == 0xFFFFFFFF )
    {
        _printf("\n[SDC ERROR] in _sdc_init() : no response to CMD3\n");
        return 1;
    }
    _sdc_rca = rsp;

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_init() : SDC_CMD3 done at cycle %d\n", _get_proctime() );
#endif

    // send CMD7
    rsp = _sdc_send_cmd( SDC_CMD7 , _sdc_rca );
    if ( rsp == 0xFFFFFFFF )
    {
        _printf("\n[SDC ERROR] in _sdc_init() : no response to CMD7\n");
        return 1;
    }

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_init() : SDC_CMD7 done at cycle %d\n", _get_proctime() );
#endif

    //////////// AHCI interface initialisation  ///////

    unsigned int       cmd_list_vaddr;
    unsigned int       cmd_table_vaddr;
    unsigned long long cmd_list_paddr;
    unsigned long long cmd_table_paddr;
    unsigned int       flags;                 // unused

    // compute Command list & command table physical addresses
    cmd_list_vaddr  = (unsigned int)(&_ahci_cmd_list[0]);
    cmd_table_vaddr = (unsigned int)(&_ahci_cmd_table[0]);
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

    // initialise Command List pointers
    _ahci_cmd_ptw = 0;
    _ahci_cmd_ptr = 0;

    // initialise Command Descriptors in Command List
    unsigned int         c;      
    unsigned long long   paddr;
    for( c=0 ; c<32 ; c++ )
    {
        paddr = cmd_table_paddr + c * sizeof(ahci_cmd_table_t);
        _ahci_cmd_list[c].ctba  = (unsigned int)(paddr);
        _ahci_cmd_list[c].ctbau = (unsigned int)(paddr>>32);
    }

    // initialise AHCI registers 
    _sdc_set_register( AHCI_PXCLB  , (unsigned int)(cmd_list_paddr) );
    _sdc_set_register( AHCI_PXCLBU , (unsigned int)(cmd_list_paddr>>32) );
    _sdc_set_register( AHCI_PXIE   , 0 );
    _sdc_set_register( AHCI_PXIS   , 0 );
    _sdc_set_register( AHCI_PXCI   , 0 );
    _sdc_set_register( AHCI_PXCMD  , 1 );

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_init() : AHCI init done at cycle %d\n", _get_proctime() );
#endif

    return 0;
} // end _sdc_init()


/////////////////////////////////////////////////////
unsigned int _sdc_access( unsigned int       use_irq, 
                          unsigned int       to_mem,
                          unsigned int       lba,
                          unsigned long long buf_paddr,
                          unsigned int       count )
{
    unsigned int procid  = _get_procid();
    unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH) - 1);
    unsigned int p       = procid & ((1<<P_WIDTH)-1);
    unsigned int iter;

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_access() : P[%d,%d,%d] enters at cycle %d\n"
        "  use_irq = %d / to_mem = %d / lba = %x / paddr = %l / count = %d\n",
        x , y , p , _get_proctime() , use_irq , to_mem , lba , buf_paddr, count );
#endif

    unsigned int       pxci;              // AHCI_PXCI register value
    unsigned int       ptw;               // command list write pointer
    unsigned int       pxis;              // AHCI_PXIS register value
    ahci_cmd_desc_t*   cmd_desc;          // command descriptor pointer   
    ahci_cmd_table_t*  cmd_table;         // command table pointer

    // check buffer alignment
    if( buf_paddr & 0x3F )
    {
        _printf("\n[SDC ERROR] in _sdc_access() : buffer not 64 bytes aligned\n");
        return 1;
    }

    // get one entry in Command List, using an
    // atomic increment on the _ahci_cmd_ptw allocator
    // only the 5 LSB bits are used to index the Command List
    ptw = _atomic_increment( &_ahci_cmd_ptw , 1 ) & 0x1F;

    // blocked until allocated entry in Command List is empty
    iter = SDC_POLLING_TIMEOUT;
    do
    {
        // get PXCI register
        pxci = _sdc_get_register( AHCI_PXCI );

        // check livelock
        iter--;
        if ( iter == 0 )
        {
            _printf("\n[SDC ERROR] in _sdc_access() : cannot get PXCI slot\n");
            return 1;
        }
    } 
    while ( pxci & (1<<ptw) );

    // compute pointers on command descriptor and command table    
    cmd_desc  = &_ahci_cmd_list[ptw];
    cmd_table = &_ahci_cmd_table[ptw];

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
    _mmc_sync( cmd_table_paddr & (~0x3F) , sizeof(ahci_cmd_table_t) );

    // update external memory for command descriptor
    _mmc_sync( cmd_desc_paddr & (~0x3F) , sizeof(ahci_cmd_desc_t) );

    // inval or synchronize memory buffer
    if ( to_mem )  _mmc_inval( buf_paddr, count<<9 );
    else           _mmc_sync( buf_paddr, count<<9 );

#endif     // end software L2/L3 cache coherence

    /////////////////////////////////////////////////////////////////////
    // In synchronous mode, we poll the PXCI register until completion
    /////////////////////////////////////////////////////////////////////
    if ( use_irq == 0 ) 
    {
        // start transfer
        _sdc_set_register( AHCI_PXCI, (1<<ptw) );

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_access() : command %d for P[%d,%d,%d]"
        " at cycle %d / polling\n",
        ptw , x , y , p , _get_proctime() );
#endif
        // disable IRQs in PXIE register
        _sdc_set_register( AHCI_PXIE , 0 );

        // poll PXCI[ptw] until command completed
        iter = SDC_POLLING_TIMEOUT;
        do
        {
            pxci = _sdc_get_register( AHCI_PXCI );

            // check livelock
            iter--;
            if ( iter == 0 )
            {
                _printf("\n[SDC ERROR] in _sdc_access() : polling PXCI timeout\n");
                return 1;
            }
        }
        while( pxci & (1<<ptw) ); 
             
        // get PXIS register
        pxis = _sdc_get_register( AHCI_PXIS );

        // reset PXIS register
        _sdc_set_register( AHCI_PXIS , 0 );
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
_printf("\n[DEBUG SDC] _sdc_access() : command %d for P[%d,%d,%d] "
        "at cycle %d / descheduling\n",
        ptw , x , y , p , _get_proctime() );
#endif
        unsigned int save_sr;
        unsigned int ltid = _get_current_task_id();

        // activates interrupt 
        _sdc_set_register( AHCI_PXIE , 0x00000001 ); 

        // set _ahci_gtid[ptw] 
        _ahci_gtid[ptw] = (procid<<16) + ltid;

        // enters critical section
        _it_disable( &save_sr ); 

        // Set NORUN_MASK_IOC bit 
        static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
        unsigned int*       ptr     = &psched->context[ltid][CTX_NORUN_ID];
        _atomic_or( ptr , NORUN_MASK_IOC );
        
        // start transfer
        _sdc_set_register( AHCI_PXCI, (1<<ptw) );

        // deschedule task
        _ctx_switch();                      

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_access() : task %d on P[%d,%d,%d] resume at cycle %d\n",
        ltid , x , y , p , _get_proctime() );
#endif

        // restore SR
        _it_restore( &save_sr );

        // get command status
        pxis = _ahci_status[ptw];
    }    

#if GIET_DEBUG_IOC
if (_get_proctime() > GIET_DEBUG_IOC)
_printf("\n[DEBUG SDC] _sdc_access() : P[%d,%d,%d] exit at cycle %d\n",
        x , y , p , _get_proctime() );
#endif

    if ( pxis & 0x40000000 ) return pxis;
    else                     return 0;

} // end _sdc_access()


///////////////////////////////////////////////////////////////////////////////
// This ISR handles the IRQ generated by the AHCI_SDC controler
///////////////////////////////////////////////////////////////////////////////
void _sdc_isr( unsigned int irq_type,
               unsigned int irq_id,
               unsigned int channel )
{
    // get AHCI_PXCI containing commands status
    unsigned int pxci = _sdc_get_register( AHCI_PXCI );

    // we must handle all completed commands 
    // active commands are between  (_ahci_cmd_ptr) and (_ahci_cmd_ptw-1) 
    unsigned int current;
    for ( current = _ahci_cmd_ptr ; current != _ahci_cmd_ptw ; current++ )
    {
        unsigned int cmd_id = current & 0x1F;
        
        if ( (pxci & (1<<cmd_id)) == 0 )    // command completed
        {
            // increment the 32 bits variable _ahci_cmd_ptr
            _ahci_cmd_ptr++;

            // save AHCI_PXIS register
            _ahci_status[cmd_id] = _sdc_get_register( AHCI_PXIS );

            // reset AHCI_PXIS register
            _sdc_set_register( AHCI_PXIS , 0 );
 
            // identify waiting task 
            unsigned int procid  = _ahci_gtid[cmd_id]>>16;
            unsigned int ltid    = _ahci_gtid[cmd_id] & 0xFFFF;
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
_printf("\n[DEBUG SDC] _sdc_isr() : command %d completed at cycle %d\n"
        "  resume task %d running on P[%d,%d,%d] / status = %x\n",
        cmd_id , _get_proctime() ,
        ltid , x , y , p , _ahci_status[cmd_id] );
#endif
        }
        else                         // command non completed
        {
            break;
        }
    }  // end for completed commands
}  // end _sdc_isr()

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
