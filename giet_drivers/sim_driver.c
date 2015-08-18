///////////////////////////////////////////////////////////////////////////////////
// File     : sim_driver.c
// Date     : 23/11/2013
// Author   : alain greiner / cesar fuguet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <hard_config.h>
#include <giet_config.h>
#include <sim_driver.h>

#if !defined(SEG_SIM_BASE) 
# error: You must define SEG_SIM_BASE in the hard_config.h file
#endif

/////////////////////////////////////////////////////
void _sim_helper_access( unsigned int register_index,
                         unsigned int value,
                         unsigned int * retval ) 
{
    volatile unsigned int* sim_helper_address = (unsigned int*)&seg_sim_base;
    
    if (register_index == SIMHELPER_SC_STOP)
    {
        sim_helper_address[register_index] = value;
    }
    else if (register_index == SIMHELPER_CYCLES) 
    {
        *retval = sim_helper_address[register_index];
    }
    else 
    {
        _tty_get_lock( 0 );
        _puts("\n[GIET ERROR] in _sim_helper_access() : undefined register\n");
        _tty_release_lock( 0 );
        _exit();
    }
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

