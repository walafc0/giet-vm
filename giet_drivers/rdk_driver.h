///////////////////////////////////////////////////////////////////////////////
// File      : rdk_driver.h
// Date      : 13/02/2014
// Author    : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////
// The rdk_driver.c and rdk_driver.h files are part ot the GIET-VM kernel.
//
// This driver supports a virtual disk implemented as a memory segment, 
// in the physical address space. 
//
// The _rdk_access() function use a software memcpy to implement both the read
// and write accesses, whatever the selected IRQ mode. It returns only when 
// the transfer is completed. The memory buffer address is a virtual address.
//
// As the number of concurrent accesses is not bounded, it does not use any lock.
//
// The SEG_RDK_BASE virtual address must be defined in the hard_config.h
// file when the USE_RAMDISK flag is set.
///////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_RDK_DRIVERS_H_
#define _GIET_RDK_DRIVERS_H_

///////////////////////////////////////////////////////////////////////////////
// Transfer data between a memory buffer and the RAMDISK. 
// - use_irq   : not used: accees is always synchronous.
// - to_mem    : to memory buffer when non zero
// - lba       : first block index on the block device
// - buf_vaddr : virtual base address of the memory buffer
// - count     : number of blocks to be transfered.
// Returns 0 if success, > 0 if error.
///////////////////////////////////////////////////////////////////////////////
extern unsigned int _rdk_access( unsigned int use_irq,
                                 unsigned int to_mem,
                                 unsigned int lba, 
                                 unsigned long long buf_vaddr,
                                 unsigned int count );

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

