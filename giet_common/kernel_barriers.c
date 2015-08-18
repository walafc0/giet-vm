//////////////////////////////////////////////////////////////////////////////
// File     : kernel_barriers.c
// Date     : 19/01/2015
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
//////////////////////////////////////////////////////////////////////////////

#include "kernel_barriers.h"
#include "giet_config.h"
#include "hard_config.h"
#include "utils.h"
#include "tty0.h"
#include "kernel_malloc.h"
#include "io.h"

///////////////////////////////////////////////////////////////////////////////
//      Simple barrier access functions
///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////
void _simple_barrier_init( simple_barrier_t*  barrier,
                           unsigned int       ntasks )
{

#if GIET_DEBUG_SIMPLE_BARRIER
unsigned int    gpid = _get_procid();
unsigned int    px   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    pl   = gpid & ((1<<P_WIDTH)-1);
_printf("[DEBUG SIMPLE_BARRIER] proc[%d,%d,%d] enters _simple_barrier_init()"
               " / vaddr = %x / ntasks = %d\n",
               px, py, pl, (unsigned int)barrier , ntasks );
#endif

    barrier->ntasks = ntasks;
    barrier->count  = ntasks;
    barrier->sense  = 0;

    asm volatile ("sync" ::: "memory");

}  // end simple_barrier_init()

//////////////////////////////////////////////////////
void _simple_barrier_wait( simple_barrier_t*  barrier )
{

#if GIET_DEBUG_SIMPLE_BARRIER
unsigned int    gpid = _get_procid();
unsigned int    px   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    pl   = gpid & ((1<<P_WIDTH)-1);
_printf("[DEBUG SIMPLE_BARRIER] proc[%d,%d,%d] enters _simple_barrier_wait()"
               " / vaddr = %x / ntasks = %d / count = %d / sense = %d\n",
               px, py, pl , (unsigned int)barrier , barrier->ntasks ,
               barrier->count , barrier->sense );
#endif

    // compute expected sense value 
    unsigned int expected;
    if ( barrier->sense == 0 ) expected = 1;
    else                       expected = 0;

    // decrement local "count"
    unsigned int count = _atomic_increment( &barrier->count, -1 );

    // the last task re-initializes count and toggle sense,
    // waking up all other waiting tasks
    if (count == 1)   // last task
    {
        barrier->count = barrier->ntasks;
        barrier->sense = expected;
    }
    else              // other tasks poll the sense flag
    {
        while ( ioread32( &barrier->sense ) != expected ) asm volatile ("nop");
    }

    asm volatile ("sync" ::: "memory");

#if GIET_DEBUG_SIMPLE_BARRIER
_printf("[DEBUG SIMPLE_BARRIER] proc[%d,%d,%d] exit simple barrier_wait()\n",
               px, py, pl );
#endif

} // end _simple_barrier_wait()




////////////////////////////////////////////////////////////////////////////////
//      SQT barrier access functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// This recursive function is called by the _sqt_barrier_init() function
// to initializes the SQT nodes (mainly the parent and child pointers).
// It traverses the SQT from top to bottom.
// The SQT can be uncomplete (when xmax or ymax are not power of 2),
// and the recursion stops when the (x,y) coordinates exceed the footprint.
////////////////////////////////////////////////////////////////////////////////
static 
void _sqt_barrier_build( sqt_barrier_t*      barrier,   // barrier pointer
                         unsigned int        x,         // node x coordinate
                         unsigned int        y,         // node y coordinate
                         unsigned int        level,     // node level
                         sqt_barrier_node_t* parent,    // parent node
                         unsigned int        xmax,     // SQT X size
                         unsigned int        ymax )   // SQT Y size
{

#if GIET_DEBUG_SQT_BARRIER
unsigned int    gpid = _get_procid();
unsigned int    px   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    pl   = gpid & ((1<<P_WIDTH)-1);
#endif

    // get target node pointer
    sqt_barrier_node_t* node = barrier->node[x][y][level];
    
    if (level == 0 )        // terminal case
    {
        // initializes target node
        node->arity    = NB_PROCS_MAX;   
        node->count    = NB_PROCS_MAX;
        node->sense    = 0;
        node->level    = 0;
        node->parent   = parent;
        node->child[0] = NULL;
        node->child[1] = NULL;
        node->child[2] = NULL;
        node->child[3] = NULL;

#if GIET_DEBUG_SQT_BARRIER
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] initialises SQT node[%d,%d,%d] :\n"
      " parent = %x / childO = %x / child1 = %x / child2 = %x / child3 = %x\n",
      px , py , pl , x , y , level , 
      (unsigned int)node->parent , 
      (unsigned int)node->child[0] , 
      (unsigned int)node->child[1] , 
      (unsigned int)node->child[2] , 
      (unsigned int)node->child[3] );
#endif

    }
    else                  // non terminal case
    {
        unsigned int cx[4];      // x coordinate for children
        unsigned int cy[4];      // y coordinate for children
        unsigned int arity = 0;  // number of children
        unsigned int i;          // child index

        // the child0 coordinates are equal to the parent coordinates
        // other child coordinates are incremented depending on the level value
        cx[0] = x;
        cy[0] = y;

        cx[1] = x + (1 << (level-1));
        cy[1] = y;

        cx[2] = x;
        cy[2] = y + (1 << (level-1));

        cx[3] = x + (1 << (level-1));
        cy[3] = y + (1 << (level-1));

        // initializes target node
        for ( i = 0 ; i < 4 ; i++ )
        {
            if ( (cx[i] < xmax) && (cy[i] < ymax) ) 
            {
                node->child[i] = barrier->node[cx[i]][cy[i]][level-1];
                arity++;
            }
            else  node->child[i] = NULL;
        }
        node->arity    = arity;  
        node->count    = arity;
        node->sense    = 0;
        node->level    = level;
        node->parent   = parent;

#if GIET_DEBUG_SQT_BARRIER
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] initialises SQT node[%d,%d,%d] : \n"
      " parent = %x / childO = %x / child1 = %x / child2 = %x / child3 = %x\n",
      px , py , pl , x , y , level , 
      (unsigned int)node->parent ,
      (unsigned int)node->child[0] , 
      (unsigned int)node->child[1] , 
      (unsigned int)node->child[2] , 
      (unsigned int)node->child[3] );
#endif

        // recursive calls for children nodes
        for ( i = 0 ; i < 4 ; i++ )
        {
            if ( (cx[i] < xmax) && (cy[i] < ymax) ) 
                _sqt_barrier_build( barrier, 
                                    cx[i], 
                                    cy[i], 
                                    level-1, 
                                    node, 
                                    xmax, 
                                    ymax );
        }
    }
}  // end _sqt_barrier_build()

///////////////////////////////////////////////////////////////////////////////
// This external function initialises the distributed SQT barrier.
// It allocates memory for the distributed SQT nodes in clusters,
// and initializes the SQT nodes pointers array (stored in cluster[0][0].
// The SQT can be "uncomplete" as SQT barrier nodes are only build in clusters
// containing processors, and contained in the mesh (X_SIZE/Y_SIZE). 
// The actual number of SQT barriers nodes in a cluster[x][y] depends on (x,y): 
// At least 1 node / at most 5 nodes per cluster:
// - barrier arbitrating between all processors of   1 cluster  has level 0,
// - barrier arbitrating between all processors of   4 clusters has level 1,
// - barrier arbitrating between all processors of  16 clusters has level 2,
// - barrier arbitrating between all processors of  64 clusters has level 3,
// - barrier arbitrating between all processors of 256 clusters has level 4,
///////////////////////////////////////////////////////////////////////////////
void _sqt_barrier_init( sqt_barrier_t*  barrier )
{
    unsigned int levels;
    unsigned int xmax;
    unsigned int ymax;

    // compute the smallest covering SQT
    _get_sqt_footprint( &xmax, &ymax, &levels );

#if GIET_DEBUG_SQT_BARRIER
unsigned int    gpid = _get_procid();
unsigned int    px   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    pl   = gpid & ((1<<P_WIDTH)-1);
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] initialises SQT barrier %x : \n"
               " xmax = %d / ymax = %d / levels = %d\n",
               px , py , pl , (unsigned int)barrier , 
               xmax , ymax , levels );
#endif

    unsigned int x;              // x coordinate for one SQT node
    unsigned int y;              // y coordinate for one SQT node
    unsigned int l;              // level for one SQT node

    for ( x = 0 ; x < xmax ; x++ )
    {
        for ( y = 0 ; y < ymax ; y++ )
        {
            for ( l = 0 ; l < levels ; l++ )            
            {
                
                if ( ( (l == 0) && ((x&0x00) == 0) && ((y&0x00) == 0) ) ||
                     ( (l == 1) && ((x&0x01) == 0) && ((y&0x01) == 0) ) ||
                     ( (l == 2) && ((x&0x03) == 0) && ((y&0x03) == 0) ) ||
                     ( (l == 3) && ((x&0x07) == 0) && ((y&0x07) == 0) ) ||
                     ( (l == 4) && ((x&0x0F) == 0) && ((y&0x0F) == 0) ) )
                 {
                     barrier->node[x][y][l] = 
                     (sqt_barrier_node_t*)_remote_malloc( sizeof(sqt_barrier_node_t),
                                                          x, y );

#if GIET_DEBUG_SQT_BARRIER
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] allocates SQT node[%d,%d,%d]"
               " : vaddr = %x\n",
               px , py , pl , x , y , l , (unsigned int)barrier->node[x][y][l] );
#endif
                 }
            }
        }
    }
            
    // recursively initialize SQT nodes from root to bottom
    _sqt_barrier_build( barrier,       // pointer on the SQT barrier descriptor
                        0,             // cluster X coordinate
                        0,             // cluster Y coordinate
                        levels-1,      // level in SQT
                        NULL,          // pointer on the parent node
                        xmax,          // SQT footprint X size
                        ymax );        // SQT footprint Y size

    asm volatile ("sync" ::: "memory");

} // end _sqt_barrier_init()

///////////////////////////////////////////////////////////////////////////////
// This recursive function is called by the _sqt_barrier_wait().
// It decrements the distributed count variables, 
// traversing the SQT from bottom to root.
// The last arrived task reset the local node before returning.
///////////////////////////////////////////////////////////////////////////////
static 
void _sqt_barrier_decrement( sqt_barrier_node_t* node )
{

#if GIET_DEBUG_SQT_BARRIER
unsigned int    gpid = _get_procid();
unsigned int    px   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    pl   = gpid & ((1<<P_WIDTH)-1);
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] decrement SQT barrier node %x :\n"
        " level = %d / arity = %d / sense = %d / count = %d\n",
        px , py , pl , (unsigned int)node , 
        node->level , node->arity , node->sense , node->count );
#endif

    // compute expected sense value
    unsigned int expected;
    if ( node->sense == 0 ) expected = 1;
    else                    expected = 0;

    // decrement local "count"
    unsigned int count = _atomic_increment( &node->count, -1 );

    if ( count == 1 )    // last task
    {
        // decrement the parent node if the current node is not the root
        if ( node->parent != NULL )      
            _sqt_barrier_decrement( node->parent );

        // reset the current node
        node->sense = expected;
        node->count = node->arity;

#if GIET_DEBUG_SQT_BARRIER
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] reset SQT barrier node %x :\n"
        " level = %d / arity = %d / sense = %d / count = %d\n",
        px , py , pl , (unsigned int)node , 
        node->level , node->arity , node->sense , node->count );
#endif
        return;
    }
    else                 // not the last task
    {
        // poll the local "sense" flag
        while ( ioread32( &node->sense ) != expected ) asm volatile ("nop");

        return;
    }
}  // end _sqt_barrier_decrement()

////////////////////// initialises NIC & CMA RX channel, and /////////////////////////////////////////////////////////
// This external blocking function waits until all procesors reach the barrier.
// Returns only when the barrier has been taken.
///////////////////////////////////////////////////////////////////////////////
void _sqt_barrier_wait( sqt_barrier_t*  barrier )
{
    // get cluster coordinates
    unsigned int gpid = _get_procid();
    unsigned int px   = (gpid >> (Y_WIDTH + P_WIDTH)) & ((1<<X_WIDTH)-1);
    unsigned int py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);

#if GIET_DEBUG_SQT_BARRIER
unsigned int pl = gpid & ((1<<P_WIDTH)-1);
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] enters SQT barrier %x at cycle %d\n",
        px , py , pl , (unsigned int)barrier , _get_proctime() ); 
#endif

   // decrement the barrier counters
    _sqt_barrier_decrement( barrier->node[px][py][0] );

#if GIET_DEBUG_SQT_BARRIER
_printf("\n[DEBUG SQT_BARRIER] P[%d,%d,%d] exit SQT barrier %x at cycle %d\n",
        px , py , pl , (unsigned int)barrier , _get_proctime() ); 
#endif

    asm volatile ("sync" ::: "memory");

}  // end _sqt_barrier_wait()



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

