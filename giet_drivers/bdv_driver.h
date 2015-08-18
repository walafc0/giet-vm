///////////////////////////////////////////////////////////////////////////////////
// File      : bdv_driver.h
// Date      : 01/11/2013
// Author    : alain greiner
// Maintainer: cesar fuguet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The bdv_driver.c and bdv_driver.h files are part ot the GIET-VM kernel.
// This driver supports the SocLib vci_block_device component, that is
// a single channel, block oriented, external storage contrôler.
//
// The _bdv_access() function supports both read and write access to block device,
// and implement two operating modes:
//
// - in "synchronous" mode, it uses a polling policy on the BDV STATUS 
//   register to detect transfer completion, as interrupts are not activated. 
//   This mode is used by the boot code to load the map.bin file into memory
//   (before MMU activation), or to load the .elf files (after MMU activation). 
// - In "descheduling" mode, ir uses a descheduling + IRQ policy.
//   The ISR executed when transfer completes should restart the calling task,
//   as the calling task global index has been saved in the _bdv_gtid variable.
//   
// As the BDV component can be used by several programs running in parallel,
// the _bdv_lock variable guaranties exclusive access to the device.
//
// The SEG_IOC_BASE address must be defined in the hard_config.h file.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_BDV_DRIVER_H_
#define _GIET_BDV_DRIVER_H_

#include "kernel_locks.h"

///////////////////////////////////////////////////////////////////////////////////
// BDV registers, operations and status values
///////////////////////////////////////////////////////////////////////////////////

enum BDV_registers 
{
    BLOCK_DEVICE_BUFFER,
    BLOCK_DEVICE_LBA,
    BLOCK_DEVICE_COUNT,
    BLOCK_DEVICE_OP,
    BLOCK_DEVICE_STATUS,
    BLOCK_DEVICE_IRQ_ENABLE,
    BLOCK_DEVICE_SIZE,
    BLOCK_DEVICE_BLOCK_SIZE,
    BLOCK_DEVICE_BUFFER_EXT,
};

enum BDV_operations 
{
    BLOCK_DEVICE_NOOP,
    BLOCK_DEVICE_READ,
    BLOCK_DEVICE_WRITE,
};

enum BDV_status
{
    BLOCK_DEVICE_IDLE,
    BLOCK_DEVICE_BUSY,
    BLOCK_DEVICE_READ_SUCCESS,
    BLOCK_DEVICE_WRITE_SUCCESS,
    BLOCK_DEVICE_READ_ERROR,
    BLOCK_DEVICE_WRITE_ERROR,
    BLOCK_DEVICE_ERROR,
};

///////////////////////////////////////////////////////////////////////////////
//           Global variables
///////////////////////////////////////////////////////////////////////////////

extern spin_lock_t  _bdv_lock;

extern unsigned int _bdv_gtid;

extern unsigned int _bdv_status;

///////////////////////////////////////////////////////////////////////////////////
//            Access functions
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// This function cheks block size == 512, and desactivates the interrupts.
// Return 0 for success, > 0 if error
///////////////////////////////////////////////////////////////////////////////////
extern unsigned int _bdv_init();

///////////////////////////////////////////////////////////////////////////////////
// Transfer data to/from the block device from/to a memory buffer. 
// - use_irq  : descheduling + IRQ if non zero / polling if zero
// - to_mem   : from external storage to memory when non 0.
// - lba      : first block index on the block device
// - buffer   : pbase address of the memory buffer (must be word aligned)
// - count    : number of blocks to be transfered.
// Returns 0 if success, > 0 if error.
///////////////////////////////////////////////////////////////////////////////////
extern unsigned int _bdv_access( unsigned int       use_irq,
                                 unsigned int       to_mem,
                                 unsigned int       lba, 
                                 unsigned long long buffer,
                                 unsigned int       count );

///////////////////////////////////////////////////////////////////////////////////
// This ISR save the status, acknowledge the IRQ, and activates the task 
// waiting on IO transfer. It can be an HWI or a SWI.
//
// TODO the _set_task_slot access should be replaced by an atomic LL/SC
//      when the CTX_RUN bool will be replaced by a bit_vector. 
///////////////////////////////////////////////////////////////////////////////////
extern void _bdv_isr( unsigned irq_type,
                      unsigned irq_id,
                      unsigned channel );


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

