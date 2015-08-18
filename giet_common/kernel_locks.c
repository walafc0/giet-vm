///////////////////////////////////////////////////////////////////////////////////
// File     : kernel_locks.c
// Date     : 01/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include "kernel_locks.h"
#include "giet_config.h"
#include "hard_config.h"
#include "utils.h"
#include "tty0.h"
#include "kernel_malloc.h"
#include "io.h"

///////////////////////////////////////////////////
unsigned int _atomic_increment( unsigned int* ptr,
                                int           increment )
{
    unsigned int value;

    asm volatile (
        "1234:                         \n"
        "move $10,   %1                \n"   /* $10 <= ptr               */
        "move $11,   %2                \n"   /* $11 <= increment         */
        "ll   $12,   0($10)            \n"   /* $12 <= *ptr              */
        "addu $13,   $11,    $12       \n"   /* $13 <= *ptr + increment  */
        "sc   $13,   0($10)            \n"   /* *ptr <= $12              */ 
        "beqz $13,   1234b             \n"   /* retry if failure         */
        "move %0,    $12               \n"   /* value <= *ptr if success */
        : "=r" (value) 
        : "r" (ptr), "r" (increment)
        : "$10", "$11", "$12", "$13", "memory" );

    return value;
}

////////////////////////////////////
void _atomic_or( unsigned int* ptr,
                 unsigned int  mask )
{
    asm volatile (
        "1789:                         \n"
        "move $10,   %0                \n"   /* $10 <= ptr               */
        "move $11,   %1                \n"   /* $11 <= mask              */
        "ll   $12,   0($10)            \n"   /* $12 <= *ptr              */
        "or   $12,   $11,    $12       \n"   /* $12 <= *ptr | mask       */
        "sc   $12,   0($10)            \n"   /* *ptr <= $12              */ 
        "beqz $12,   1789b             \n"   /* retry if failure         */
        "nop                           \n"  
        :
        : "r" (ptr), "r" (mask)
        : "$10", "$11", "$12", "memory" );
}

////////////////////////////////////
void _atomic_and( unsigned int* ptr,
                  unsigned int  mask )
{
    asm volatile (
        "1945:                         \n"
        "move $10,   %0                \n"   /* $10 <= ptr               */
        "move $11,   %1                \n"   /* $11 <= mask              */
        "ll   $12,   0($10)            \n"   /* $12 <= *ptr              */
        "and  $12,   $11,    $12       \n"   /* $13 <= *ptr & mask       */
        "sc   $12,   0($10)            \n"   /* *ptr <= new              */ 
        "beqz $12,   1945b             \n"   /* retry if failure         */
        "nop                           \n"  
        :
        : "r" (ptr), "r" (mask)
        : "$10", "$11", "$12", "memory" );
}


///////////////////////////////////////////////////////////////////////////////////
//      Simple lock access functions
///////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////
void _simple_lock_acquire( simple_lock_t* lock )
{

#if GIET_DEBUG_SIMPLE_LOCK
unsigned int    gpid = _get_procid();
unsigned int    x    = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    y    = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    l    = gpid & ((1<<P_WIDTH)-1);
_nolock_printf("\n[DEBUG SIMPLE_LOCK] P[%d,%d,%d] enters acquire() at cycle %d\n",
               x , y , l , _get_proctime() );
#endif

    asm volatile ( "1515:                   \n"
	               "lw   $2,    0(%0)       \n"
	               "bnez $2,    1515b       \n"
                   "ll   $2,    0(%0)       \n"
                   "bnez $2,    1515b       \n"
                   "li   $3,    1           \n"
                   "sc   $3,    0(%0)       \n"
                   "beqz $3,    1515b       \n"
                   :
                   : "r"(lock)
                   : "$2", "$3", "memory" );

#if GIET_DEBUG_SIMPLE_LOCK
_nolock_printf("\n[DEBUG SIMPLE_LOCK] P[%d,%d,%d] exit acquire() at cycle %d\n",
               x , y , l , _get_proctime() );
#endif

}

////////////////////////////////////////////////
void _simple_lock_release( simple_lock_t* lock )
{
    asm volatile ( "sync" );   // for consistency

    lock->value = 0;

#if GIET_DEBUG_SIMPLE_LOCK
unsigned int    gpid = _get_procid();
unsigned int    x    = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    y    = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    l    = gpid & ((1<<P_WIDTH)-1);
_nolock_printf("\n[DEBUG SIMPLE_LOCK] P[%d,%d,%d] release() at cycle %d\n",
               x , y , l , _get_proctime() );
#endif

}


///////////////////////////////////////////////////////////////////////////////////
//      Queuing Lock access functions
///////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////
void _spin_lock_init( spin_lock_t* lock )
{
    lock->current = 0;
    lock->free    = 0;

#if GIET_DEBUG_SPIN_LOCK
unsigned int    gpid = _get_procid();
unsigned int    x    = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    y    = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    l    = gpid & ((1<<P_WIDTH)-1);
_nolock_printf("\n[DEBUG SPIN_LOCK] P[%d,%d,%d] initializes lock %x at cycle %d\n",
               x, y, l, (unsigned int)lock, _get_proctime() );
#endif

}


////////////////////////////////////////////
void _spin_lock_acquire( spin_lock_t* lock )
{
    // get next free slot index fromlock
    unsigned int ticket = _atomic_increment( &lock->free, 1 );

#if GIET_DEBUG_SPIN_LOCK
unsigned int    gpid = _get_procid();
unsigned int    x    = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    y    = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    l    = gpid & ((1<<P_WIDTH)-1);
_nolock_printf("\n[DEBUG SPIN_LOCK] P[%d,%d,%d] get ticket %d for lock %x at cycle %d"
               " / current = %d / free = %d\n",
               x, y, l, ticket, (unsigned int)lock, _get_proctime(),
               lock->current, lock->free );
#endif

    // poll the spin_lock current slot index
    while ( ioread32( &lock->current ) != ticket ) asm volatile ("nop");

#if GIET_DEBUG_SPIN_LOCK
_nolock_printf("\n[DEBUG SPIN_LOCK] P[%d,%d,%d] get lock %x at cycle %d"
               " / current = %d / free = %d\n",
               x, y, l, (unsigned int)lock, _get_proctime(),
               lock->current, lock->free );
#endif

}

////////////////////////////////////////////
void _spin_lock_release( spin_lock_t* lock )
{
    asm volatile ( "sync" );   // for consistency

    lock->current = lock->current + 1;

#if GIET_DEBUG_SPIN_LOCK
_nolock_printf("\n[DEBUG SPIN_LOCK] P[%d,%d,%d] release lock %x at cycle %d"
               " / current = %d / free = %d\n",
               x, y, l, (unsigned int)lock, _get_proctime(),
               lock->current, lock->free );
#endif

}



///////////////////////////////////////////////////////////////////////////////////
//      SQT lock access functions
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// This recursive function is used by the _sqt_lock_init() function
// to initializes the SQT nodes (mainly the parent and child pointers).
// It traverses the SQT from top to bottom.
// The SQT can be uncomplete (when xmax or ymax are not power of 2),
// and the recursion stops when the (x,y) coordinates exceed the footprint.
///////////////////////////////////////////////////////////////////////////////////
static 
void _sqt_lock_build( sqt_lock_t*      lock,      // pointer on the SQT lock
                      unsigned int     x,         // node X coordinate
                      unsigned int     y,         // node Y coordinate
                      unsigned int     level,     // node level
                      sqt_lock_node_t* parent,    // pointer on parent node
                      unsigned int     xmax,      // SQT X size
                      unsigned int     ymax )     // SQT Y size
{

#if GIET_DEBUG_SQT_LOCK
unsigned int    gpid = _get_procid();
unsigned int    px   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    pl   = gpid & ((1<<P_WIDTH)-1);
#endif

    // get target node pointer
    sqt_lock_node_t* node = lock->node[x][y][level];
    
    if (level == 0 )        // terminal case
    {
        // initializes target node
        node->current  = 0;   
        node->free     = 0;
        node->level    = 0;
        node->parent   = parent;
        node->child[0] = NULL;
        node->child[1] = NULL;
        node->child[2] = NULL;
        node->child[3] = NULL;

#if GIET_DEBUG_SQT_LOCK
_nolock_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] initialises SQT node[%d,%d,%d] : \n"
      " parent = %x / childO = %x / child1 = %x / child2 = %x / child3 = %x\n",
      px , py , pl , x , y , level , 
      (unsigned int)node->parent , 
      (unsigned int)node->child[0] , 
      (unsigned int)node->child[1] , 
      (unsigned int)node->child[2] , 
      (unsigned int)node->child[3] );
#endif

    }
    else                   // non terminal case
    {
        unsigned int cx[4];      // x coordinate for children
        unsigned int cy[4];      // y coordinate for children
        unsigned int i;          // child index

        // the child0 coordinates are equal to the parent coordinates
        // other childs coordinates are incremented depending on the level value
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
                node->child[i] = lock->node[cx[i]][cy[i]][level-1];
            else  
                node->child[i] = NULL;
        }
        node->current  = 0;
        node->free     = 0;
        node->level    = level;
        node->parent   = parent;

#if GIET_DEBUG_SQT_LOCK
_nolock_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] initialises SQT node[%d,%d,%d] : \n"
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
                _sqt_lock_build( lock, 
                                 cx[i], 
                                 cy[i], 
                                 level-1, 
                                 node, 
                                 xmax, 
                                 ymax );
        }
    }
}  // end _sqt_lock_build()

/////////////////////////////////////////////////////////////////////////////////
// This external function initialises the distributed SQT lock.
// It allocates memory for the distributed SQT nodes in clusters,
// and initializes the SQT nodes pointers array (stored in cluster[0][0].
// The SQT can be "uncomplete" as SQT lock nodes are only build in clusters
// containing processors.
// The actual number of SQT locks nodes in a cluster[x][y] depends on (x,y): 
// At least 1 node / at most 5 nodes per cluster:
// - lock arbitrating between all processors of   1 cluster  has level 0,
// - lock arbitrating between all processors of   4 clusters has level 1,
// - lock arbitrating between all processors of  16 clusters has level 2,
// - lock arbitrating between all processors of  64 clusters has level 3,
// - lock arbitrating between all processors of 256 clusters has level 4,
/////////////////////////////////////////////////////////////////////////////////
void _sqt_lock_init( sqt_lock_t*  lock )
{
    unsigned int levels;
    unsigned int xmax;
    unsigned int ymax;

    // compute the smallest SQT covering all processors
    _get_sqt_footprint( &xmax, &ymax, &levels );


#if GIET_DEBUG_SQT_LOCK
unsigned int    gpid = _get_procid();
unsigned int    px   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    py   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    pl   = gpid & ((1<<P_WIDTH)-1);
_nolock_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] initialises SQT lock %x : \n"
               " xmax = %d / ymax = %d / levels = %d\n",
               px , py , pl , (unsigned int)lock , 
               xmax , ymax , levels );
#endif

    
    unsigned int x;              // x coordinate for one SQT node
    unsigned int y;              // y coordinate for one SQT node
    unsigned int l;              // level for one SQT node

    for ( x = 0 ; x < xmax ; x++ )
    {
        for ( y = 0 ; y < ymax ; y++ )
        {
            for ( l = 0 ; l < levels ; l++ )             // level 0 nodes
            {
                
                if ( ( (l == 0) && ((x&0x00) == 0) && ((y&0x00) == 0) ) ||
                     ( (l == 1) && ((x&0x01) == 0) && ((y&0x01) == 0) ) ||
                     ( (l == 2) && ((x&0x03) == 0) && ((y&0x03) == 0) ) ||
                     ( (l == 3) && ((x&0x07) == 0) && ((y&0x07) == 0) ) ||
                     ( (l == 4) && ((x&0x0F) == 0) && ((y&0x0F) == 0) ) )
                 {
                     lock->node[x][y][l] = 
                     (sqt_lock_node_t*)_remote_malloc( sizeof(sqt_lock_node_t),
                                                       x, y );

#if GIET_DEBUG_SQT_LOCK
_nolock_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] allocates SQT node[%d,%d,%d]"
               " : vaddr = %x\n",
               px , py , pl , x , y , l , (unsigned int)lock->node[x][y][l] );
#endif
                 }
            }
        }
    }
            
    // recursively initialize all SQT nodes from root to bottom
    _sqt_lock_build( lock,       // pointer on the SQT lock descriptor
                     0,          // x coordinate
                     0,          // y coordinate
                     levels-1,   // level in SQT
                     NULL,       // pointer on the parent node
                     xmax,       // SQT footprint X size
                     ymax );     // SQT footprint X size

    asm volatile ("sync" ::: "memory");

#if GIET_DEBUG_SQT_LOCK
_nolock_printf("\n[DEBUG SQT_LOCK] SQT nodes initialisation completed\n"); 
#endif

} // end _sqt_lock_init()

//////////////////////////////////////////////////////////////////////////////////
// This recursive function is used by the sqt_lock_acquire() function to get
// a distributed SQT lock: It tries to get each local queuing lock on the path
// from bottom to top, and starting from bottom.
// It is blocking : it polls each "partial lock until it can be taken. 
// The lock is finally obtained when all locks, at all levels are taken.
//////////////////////////////////////////////////////////////////////////////////
static 
void _sqt_lock_take( sqt_lock_node_t* node )
{
    // get next free ticket from local lock
    unsigned int ticket = _atomic_increment( &node->free, 1 );

#if GIET_DEBUG_SQT_LOCK
unsigned int    gpid = _get_procid();
unsigned int    x    = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    y    = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    l    = gpid & ((1<<P_WIDTH)-1);
_nolock_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] get ticket %d for SQT lock %x"
               " / level = %d / current = %d / free = %d\n",
               x , y , l , ticket , (unsigned int)node ,
               node->level , node->current , node->free );
#endif

    // poll the local lock current index
    while ( ioread32( &node->current ) != ticket ) asm volatile( "nop" );

#if GIET_DEBUG_SQT_LOCK
_nolock_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] get SQT lock %x"
               " / level = %d / current = %d / free = %d\n",
               x , y , l , (unsigned int)node ,
               node->level , node->current , node->free );
#endif

    // try to take the parent node lock until top is reached
    if ( node->parent != NULL ) _sqt_lock_take( node->parent );

} // end _sqt_lock_take()
    
//////////////////////////////////////////////////////////////////////////////////
// This external function get thes SQT lock.
// Returns only when the lock has been taken. 
/////////////////////////////////////////////////////////////////////////////////
void _sqt_lock_acquire( sqt_lock_t*  lock )
{
    // get cluster coordinates
    unsigned int gpid = _get_procid();
    unsigned int x    = (gpid >> (Y_WIDTH + P_WIDTH)) & ((1<<X_WIDTH)-1);
    unsigned int y    = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);

    // try to recursively take the distributed locks (from bottom to top)
    _sqt_lock_take( lock->node[x][y][0] );
}


/////////////////////////////////////////////////////////////////////////////////
// This recursive function is used by the sqt_lock_release() function to
// release distributed SQT lock: It releases all local locks on the path from 
// bottom to top, using a normal read/write, and starting from bottom.
/////////////////////////////////////////////////////////////////////////////////
static 
void _sqt_lock_give( sqt_lock_node_t* node )
{
    // release the local lock
    node->current = node->current + 1;

#if GIET_DEBUG_SQT_LOCK
unsigned int    gpid = _get_procid();
unsigned int    x   = gpid >> (Y_WIDTH + P_WIDTH);
unsigned int    y   = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int    l   = gpid & ((1<<P_WIDTH)-1);
_nolock_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] release SQT lock %x"
               " / level = %d / current = %d / free = %d\n",
               x , y , l , (unsigned int)node, 
               node->level , node->current , node->free );
#endif

    // reset parent node until top is reached
    if ( node->parent != NULL ) _sqt_lock_give( node->parent );

} // end _sqt_lock_give()


/////////////////////////////////////////////////////////////////////////////////
// This external function releases the SQT lock.
/////////////////////////////////////////////////////////////////////////////////
void _sqt_lock_release( sqt_lock_t*  lock )
{
    asm volatile ( "sync" );   // for consistency

    // get cluster coordinates
    unsigned int gpid = _get_procid();
    unsigned int x    = (gpid >> (Y_WIDTH + P_WIDTH)) & ((1<<X_WIDTH)-1);
    unsigned int y    = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);

    // recursively reset the distributed locks (from bottom to top)
    _sqt_lock_give( lock->node[x][y][0] );
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

