
#ifndef USER_LOCKS_H
#define USER_LOCKS_H

#include "hard_config.h"

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

extern void sqt_lock_init( sqt_lock_t*  lock,
                       unsigned int         x_size,
                       unsigned int         y_size,
                       unsigned int         ntasks );

extern void sqt_lock_acquire( sqt_lock_t*  lock );

extern void sqt_lock_release( sqt_lock_t*  lock );


#endif
