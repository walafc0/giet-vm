///////////////////////////////////////////////////////////////////////////////////
// File     : sim_driver.h
// Date     : 23/11/2013
// Author   : alain greiner / cesar fuguet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The sim_driver.c and sim_driver.h files are part ot the GIET-VM nano-kernel.
//
// This driver supports the vci_sim_helper component, that is a pseudo hardware
// component available in the SoCLib library, and providing a monitoring service
// in a virtual prototyping environment.
//
// There is at most one such component in the architecture.
//
// The SEG_SIM_BASE address must be defined in the hard_config.h file.
////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_SIM_DRIVERS_H_
#define _GIET_SIM_DRIVERS_H_

///////////////////////////////////////////////////////////////////////////////////
// SIM_HELPER registers offsets 
///////////////////////////////////////////////////////////////////////////////////

enum SoclibSimhelperRegisters
{
    SIMHELPER_SC_STOP,                 // stop simulation
    SIMHELPER_END_WITH_RETVAL,         // Not supported
    SIMHELPER_EXCEPT_WITH_VAL,         // Not supported
    SIMHELPER_PAUSE_SIM,               // Not supported
    SIMHELPER_CYCLES,                  // Return number of cycles
    SIMHELPER_SIGINT,                  // Not supported
};

////////////////////////////////////////////////////////////////////////////////
// Accesses the Simulation Helper Component.
// - If the access is on a write register, the simulation will stop.
// - If the access is on a read register, value is written in retval buffer.
////////////////////////////////////////////////////////////////////////////////
extern void _sim_helper_access( unsigned int  register_index,
                                unsigned int  value,
                                unsigned int* retval ); 


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

