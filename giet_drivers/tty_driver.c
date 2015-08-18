///////////////////////////////////////////////////////////////////////////////////
// File     : tty_driver.c
// Date     : 23/05/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <tty_driver.h>
#include <xcu_driver.h>
#include <ctx_handler.h>
#include <utils.h>
#include <tty0.h>

#if !defined(SEG_TTY_BASE)
# error: You must define SEG_TTY_BASE in the hard_config.h file
#endif

////////////////////////////////////////////////////////////////////////////////////
//               global variables
////////////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
unsigned int   _tty_rx_buf[NB_TTY_CHANNELS];

__attribute__((section(".kdata")))
unsigned int   _tty_rx_full[NB_TTY_CHANNELS]; 

////////////////////////////////////////////////////////////////////////////////////
//               access functions
////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////
unsigned int _tty_get_register( unsigned int channel,
                                unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_TTY_BASE + channel*TTY_SPAN + index;
    return _io_extended_read( vaddr );
}

/////////////////////////////////////////////
void _tty_set_register( unsigned int channel,
                        unsigned int index,
                        unsigned int value )
{
    unsigned int* vaddr = (unsigned int*)SEG_TTY_BASE + channel*TTY_SPAN + index;
    _io_extended_write( vaddr, value );
}

//////////////////////////////////////
void _tty_init( unsigned int channel )
{
    _tty_rx_full[channel] = 0;
}

////////////////////////////////////////
void _tty_rx_isr( unsigned int irq_type,   // HWI / WTI
                  unsigned int irq_id,     // index returned by XCU
                  unsigned int channel )   // TTY channel
{
    // transfer character to kernel buffer and acknowledge TTY IRQ
    _tty_rx_buf[channel]  = _tty_get_register( channel, TTY_READ ); 

    // flush pending memory writes
    asm volatile( "sync" );

    // set kernel buffer status
    _tty_rx_full[channel] = 1;

#if GIET_DEBUG_IRQS  // we don't take the TTY lock to avoid deadlock
unsigned int gpid           = _get_procid();
unsigned int cluster_xy     = gpid >> P_WIDTH;
unsigned int x              = cluster_xy >> Y_WIDTH;
unsigned int y              = cluster_xy & ((1<<Y_WIDTH)-1);
unsigned int lpid           = gpid & ((1<<P_WIDTH)-1);
_puts("\n[IRQS DEBUG] Processor[");
_putd(x );
_puts(",");
_putd(y );
_puts(",");
_putd(lpid );
_puts("] enters _tty_rx_isr() at cycle ");
_putd(_get_proctime() );
_puts("\n  read byte = ");
_putx(_tty_rx_buf[channel] );
_puts("\n");
#endif

}

/////////////////////////////////////////
void _tty_tx_isr( unsigned int irq_type,   // HWI / WTI
                  unsigned int irq_id,     // index returned by XCU
                  unsigned int channel )   // TTY channel
{
    _puts("\n[GIET ERROR] the _tty_tx_isr() is not implemented\n");
    _exit();
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

