//////////////////////////////////////////////////////////////////////////////////
// File     : user_barrier.h         
// Date     : 01/04/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The barrier.c and barrier.h files are part of the GIET-VM nano-kernel.
// This user-level library provides a synchronisation service between several
// tasks sharing the same address space in a parallel multi-tasks application.
//
// There is actually two types of barriers:
// 1) The "giet_barrier_t" is a simple sense-reversing barrier.
//    It can be safely used several times (in a loop for example),
//    but it does not scale, and should not be used when the number
//    of tasks is larger than few tens.
//
// 2) The giet_sqt_barrier_t" can be used in multi-clusters architectures,
//    and is implemented as a physically distributed Synchro-Quad-Tree (SQT).
//    WARNING: The following placement constraints must be respected:
//    - The number of involved tasks in a cluster is the same in all clusters.
//    - The involved clusters form a mesh[x_size * y_size]
//    - The lower left involved cluster is cluster(0,0)  
//
// Neither the barrier_init(), nor the barrier_wait() function require a syscall.
// For both types of barriers, the barrier initialisation should be done by
// one single task.
///////////////////////////////////////////////////////////////////////////////////

#ifndef USER_BARRIER_H_
#define USER_BARRIER_H_

///////////////////////////////////////////////////////////////////////////////////
//  simple barrier structure and access functions
///////////////////////////////////////////////////////////////////////////////////

typedef struct giet_barrier_s 
{
    unsigned int sense;      // barrier state (toggle)
    unsigned int arity;      // total number of expected tasks
    unsigned int count;      // number of not arrived tasks
} giet_barrier_t;

///////////////////////////////////////////////////
extern void barrier_init( giet_barrier_t* barrier,
                          unsigned int    ntasks );   

////////////////////////////////////////////////////
extern void barrier_wait( giet_barrier_t* barrier );

//////////////////////////////////////////////////////////////////////////////////
// SQT barrier structures and access functions
//////////////////////////////////////////////////////////////////////////////////

typedef struct sqt_node_s 
{
    unsigned int       arity;        // number of expected tasks
    unsigned int       count;        // number of not arrived tasks
    unsigned int       sense;        // barrier state (toggle)
    unsigned int       level;        // hierarchical level (0 is bottom)
    struct sqt_node_s* parent;       // pointer on parent node (NULL for root)
    struct sqt_node_s* child[4];     // pointer on children node (NULL for bottom)
    unsigned int       padding[7];   // for 64 bytes alignment
} sqt_node_t;

typedef struct giet_sqt_barrier_s 
{
    sqt_node_t*     node[16][16][5];    // array of pointers on SQT nodes 
} giet_sqt_barrier_t;

///////////////////////////////////////////////////////////
extern void sqt_barrier_init( giet_sqt_barrier_t*  barrier,
                              unsigned int         x_size,
                              unsigned int         y_size,
                              unsigned int         ntasks );   

/////////////////////////////////////////////////////////////
extern void sqt_barrier_wait( giet_sqt_barrier_t*  barrier );


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

