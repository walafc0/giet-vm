//////////////////////////////////////////////////////////////////////////////////
// File     : kernel_malloc.h
// Date     : 05/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
//////////////////////////////////////////////////////////////////////////////////
// The kernel_malloc.c and kernel_malloc.h files are part of the giet_vm kernel.
//////////////////////////////////////////////////////////////////////////////////

#ifndef KERNEL_MALLOC_H_
#define KERNEL_MALLOC_H_

#include "kernel_locks.h"
#include "hard_config.h"


#define MIN_BLOCK_SIZE      0x40


//////////////////////////////////////////////////////////////////////////////////
//             heap descriptor (one per cluster)
//////////////////////////////////////////////////////////////////////////////////

typedef struct kernel_heap_s
{
    spin_lock_t    lock;            // lock protecting exclusive access
    unsigned int   heap_base;       // heap base address
    unsigned int   heap_size;       // heap size (bytes)
    unsigned int   alloc_base;      // alloc[] array base address
    unsigned int   alloc_size;      // alloc[] array size (bytes)
    unsigned int   free[32];        // array of base addresses of free blocks 
                                    // (address of first block of a given size)
} kernel_heap_t;

//////////////////////////////////////////////////////////////////////////////////
//             global variables
//////////////////////////////////////////////////////////////////////////////////

extern kernel_heap_t  kernel_heap[X_SIZE][Y_SIZE];

//////////////////////////////////////////////////////////////////////////////////
//  access functions
//////////////////////////////////////////////////////////////////////////////////

extern void* _malloc( unsigned int size );

extern void* _remote_malloc( unsigned int size, 
                             unsigned int x,
                             unsigned int y );

extern void _free( void* ptr );

extern void _heap_init();


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

