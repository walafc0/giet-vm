///////////////////////////////////////////////////////////////////////////////////
// File     : tty_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The tty_driver.c and tty_drivers.h files are part ot the GIET-VM kernel.
// This driver supports the SocLib vci_multi_tty component.
//
// The total number of TTY terminals must be defined by the configuration 
// parameter NB_TTY_CHANNELS in the hard_config.h file.
//
// The "system" terminal is TTY[0].
// The "user" TTYs are allocated to applications requesting it.
//
// The SEG_TTY_BASE address must be defined in the hard_config.h file.
//
// All physical accesses to device registers are done by the two
// _tty_get_register(), _tty_set_register() low-level functions,
// that are handling virtual / physical addressing.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_TTY_DRIVERS_H_
#define _GIET_TTY_DRIVERS_H_

#include "hard_config.h"
#include "kernel_locks.h"

///////////////////////////////////////////////////////////////////////////////////
//                     registers offsets
///////////////////////////////////////////////////////////////////////////////////

enum TTY_registers 
{
    TTY_WRITE   = 0,
    TTY_STATUS  = 1,
    TTY_READ    = 2,
    TTY_CONFIG  = 3,
    /**/
    TTY_SPAN    = 4,
};

//////////////////////////////////////////////////////////////////////////////////
//                    access functions
//////////////////////////////////////////////////////////////////////////////////

extern unsigned int _tty_get_register( unsigned int channel,
                                       unsigned int index );

extern void _tty_set_register( unsigned int channel,
                               unsigned int index,
                               unsigned int value );

extern void _tty_init( unsigned int channel );

//////////////////////////////////////////////////////////////////////////////////
//                 Interrupt Service Routine 
///////////////////////////////////////////////////////////////////////////////////

extern void _tty_rx_isr( unsigned int irq_type,
                         unsigned int irq_id,
                         unsigned int channel );

extern void _tty_tx_isr( unsigned int irq_type,
                         unsigned int irq_id,
                         unsigned int channel );


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

