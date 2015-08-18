////////////////////////////////////////////////////////////////////////////////
// File     : malloc.h
// Date     : 05/03/2013
// Author   : Jean-Baptiste Bréjon / alain greiner
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////
// Initialisation policy:
//   The user application must initialize the heap(x,y) structure before
//   using the malloc() or remote_malloc() functions, and this initialization
//   must be done by a single task.
////////////////////////////////////////////////////////////////////////////////
// Free blocks organisation:
// - All free blocks have a size that is a power of 2, larger or equal
//   to MIN_BLOCK_SIZE (typically 128 bytes).
// - All free blocks are aligned.
// - They are pre-classed in NB_SIZES linked lists, where all blocks in a
//   given list have the same size. 
// - The NEXT pointer implementing those linked lists is written 
//   in the 4 first bytes of the block itself, using the unsigned int type.
// - The pointers on the first free block for each size are stored in an
//   array of pointers free[32] in the heap(x,y) descriptor.
////////////////////////////////////////////////////////////////////////////////
// Allocation policy:
// - The block size required by the user can be any value, but the allocated
//   block size can be larger than the requested size:
// - The allocator computes actual_size, that is the smallest power of 2 
//   value larger or equal to the requested size AND larger or equal to
//   MIN_BLOCK_SIZE.
// - It pop the linked list of free blocks corresponding to actual_size,
//   and returns the block B if the list[actual_size] is not empty.
// - If the list[actual_size] is empty, it pop the list[actual_size * 2].
//   If a block B' is found, it break this block in 2 B/2 blocks, returns 
//   the first B/2 block and push the other B/2 block into list[actual_size].
// - If the list[actual_size * 2] is empty, it pop the list[actual_size * 4].
//   If a block B is found, it break this block in 3 blocks B/4, B/4 and B/2,
//   returns the first B/4 block, push the other blocks B/4 and B/2 into
//   the proper lists. etc... 
// - If no block satisfying the request is available it returns a failure
//   (NULL pointer).
// - This allocation policy has the nice following property:
//   If the heap segment is aligned (the heap_base is a multiple of the
//   heap_size), all allocated blocks are aligned on the actual_size.
////////////////////////////////////////////////////////////////////////////////
// Free policy:
// - Each allocated block is registered in an alloc[] array of unsigned char.
// - This registration is required by the free() operation, because the size
//   of the allocated block must be obtained from the base address of the block.  
// - The number of entries in this array is equal to the max number
//   of allocated block is : heap_size / 128.
// - For each allocated block, the value registered in the alloc[] array
//   is log2( size_of_allocated_block ).
// - The index in this array is computed from the allocated block base address:
//      index = (block_base - heap_base) / MIN_BLOCK_SIZE
// - The alloc[] array is stored at the end of heap segment. This consume
//   (1 / MIN_BLOCK_SIZE) of the total heap storage capacity.
////////////////////////////////////////////////////////////////////////////////

#ifndef _MALLOC_H_
#define _MALLOC_H_

#include "giet_config.h"
#include "user_lock.h"

////////////////////////////////////////////////////////////////////////////////
//  magic number indicating that heap(x,y) has been initialized.
////////////////////////////////////////////////////////////////////////////////

#define HEAP_INITIALIZED    0xDEADBEEF

#define MIN_BLOCK_SIZE      0x80

////////////////////////////////////////////////////////////////////////////////
// heap(x,y) descriptor (one per cluster)
////////////////////////////////////////////////////////////////////////////////

typedef struct giet_heap_s
{
    user_lock_t    lock;            // lock protecting exclusive access
    unsigned int   init;            // initialised <=> value == HEAP_INITIALIZED
    unsigned int   x;               // cluster X coordinate
    unsigned int   y;               // cluster Y coordinate
    unsigned int   heap_base;       // heap base address
    unsigned int   heap_size;       // heap size (bytes)
    unsigned int   alloc_base;      // alloc[] array base address
    unsigned int   alloc_size;      // alloc[] array size (bytes)
    unsigned int   free[32];        // array of base addresses of free blocks 
                                    // (address of first block of a given size)
} giet_heap_t;

///////// user functions /////////////////

extern void heap_init( unsigned int x,
                       unsigned int y );

extern void* malloc( unsigned int size );

extern void* remote_malloc( unsigned int size, 
                            unsigned int x,
                            unsigned int y );

extern void free(void * ptr);

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

