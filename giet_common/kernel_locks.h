//////////////////////////////////////////////////////////////////////////////
// File     : kernel_locks.h
// Date     : 01/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
//////////////////////////////////////////////////////////////////////////////
// The locks.c and locks.h files are part of the GIET-VM nano-kernel.
// They define both atomic increment operations and three types of locks.
//////////////////////////////////////////////////////////////////////////////

#ifndef GIET_LOCKS_H
#define GIET_LOCKS_H

#include "hard_config.h"


//////////////////////////////////////////////////////////////////////////////
//      Atomic access functions using LL/SC instructions
//////////////////////////////////////////////////////////////////////////////

extern unsigned int _atomic_increment( unsigned int* ptr,
                                       int  increment );

extern void _atomic_or( unsigned int* ptr,
                        unsigned int  mask );

extern void _atomic_and( unsigned int* ptr,
                         unsigned int  mask );

//////////////////////////////////////////////////////////////////////////////
//      Simple lock structure and access functions
//////////////////////////////////////////////////////////////////////////////

typedef struct simple_lock_s
{
    unsigned int value;          // lock taken if non zero
    unsigned int padding[15];    // for 64 bytes alignment
} simple_lock_t;

extern void _simple_lock_acquire( simple_lock_t* lock );

extern void _simple_lock_release( simple_lock_t* lock );

//////////////////////////////////////////////////////////////////////////////
//      Queuing lock structure and access functions
//////////////////////////////////////////////////////////////////////////////

typedef struct spin_lock_s 
{
    unsigned int current;        // current slot index:
    unsigned int free;           // next free tiket index
    unsigned int padding[14];    // for 64 bytes alignment
} spin_lock_t;

extern void _spin_lock_init( spin_lock_t* lock );

extern void _spin_lock_acquire( spin_lock_t* lock );

extern void _spin_lock_release( spin_lock_t* lock );


/////////////////////////////////////////////////////////////////////////////
//      SQT lock structures and access functions
/////////////////////////////////////////////////////////////////////////////

typedef struct sqt_lock_node_s 
{
    unsigned int            current;         // current ticket index
    unsigned int            free;            // next free ticket index
    unsigned int            level;           // hierarchical level (0 is bottom)
    struct sqt_lock_node_s* parent;          // parent node (NULL for root)
    struct sqt_lock_node_s* child[4];        // children node
    unsigned int            padding[8];      // for 64 bytes alignment         
} sqt_lock_node_t;

typedef struct sqt_lock_s 
{
    sqt_lock_node_t* node[X_SIZE][Y_SIZE][5];  // array of pointers on SBT nodes 
} sqt_lock_t;

extern void _sqt_lock_init( sqt_lock_t*   lock );

extern void _sqt_lock_acquire( sqt_lock_t*  lock );

extern void _sqt_lock_release( sqt_lock_t*  lock );


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

