///////////////////////////////////////////////////////////////////////////////////
// File     : kernel_barrier.h
// Date     : 19/01/2015
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The kernel_barrier.c and kernel_barrier.h files are part of the GIET-VM kernel.
// They define both a simple barrier and a scalable, distributed barrier,
// based on Synchronisation Quad Tree (SQT).
///////////////////////////////////////////////////////////////////////////////////

#ifndef GIET_KERNEL_BARRIERS_H
#define GIET_KERNEL_BARRIERS_H

#include "hard_config.h"

#define SBT_MAX_LEVELS 5

///////////////////////////////////////////////////////////////////////////////////
//      Simple barrier structure and access functions
///////////////////////////////////////////////////////////////////////////////////

typedef struct simple_barrier_s
{
    unsigned int sense;          // barrier state (toggle)
    unsigned int ntasks;         // total numer of expected tasks
    unsigned int count;          // number of not arrived tasks
    unsigned int padding[13];    // for 64 bytes alignment
} simple_barrier_t;

extern void _simple_barrier_init( simple_barrier_t*  barrier,
                                  unsigned int       ntasks );

extern void _simple_barrier_wait( simple_barrier_t*  barrier );

//////////////////////////////////////////////////////////////////////////////////
//      SQT barrier structures and access functions
//////////////////////////////////////////////////////////////////////////////////

typedef struct sqt_barrier_node_s 
{
    unsigned int                arity;        // number of children (4 max)
    unsigned int                count;        // number of not arrived children
    unsigned int                sense;        // barrier state (toggle)      
    unsigned int                level;        // hierarchical level (0 is bottom)
    struct sqt_barrier_node_s*  parent;       // parent node (NULL for root)
    struct sqt_barrier_node_s*  child[4];     // children node
    unsigned int                padding[7];   // for 64 bytes alignment         
} sqt_barrier_node_t;

typedef struct sqt_barrier_s 
{
    unsigned int          ntasks;
    sqt_barrier_node_t*   node[X_SIZE][Y_SIZE][SBT_MAX_LEVELS];
} sqt_barrier_t;

extern void _sqt_barrier_init( sqt_barrier_t*  barrier );

extern void _sqt_barrier_wait( sqt_barrier_t*  barrier );


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

