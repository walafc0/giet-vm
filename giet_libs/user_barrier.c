//////////////////////////////////////////////////////////////////////////////////
// File     : user_barrier.c      
// Date     : 01/04/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include "user_barrier.h"
#include "malloc.h"
#include "stdio.h"
#include "giet_config.h"

///////////////////////////////////////////////////////////////////////////////////
//      Simple barrier access functions
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////
void barrier_init( giet_barrier_t* barrier, 
                   unsigned int    ntasks ) 
{
    barrier->arity  = ntasks;
    barrier->count  = ntasks;
    barrier->sense  = 0;

    asm volatile ("sync" ::: "memory");
}

////////////////////////////////////////////
void barrier_wait( giet_barrier_t* barrier ) 
{

#if GIET_DEBUG_USER_BARRIER
unsigned int x;
unsigned int y;
unsigned int p;
giet_proc_xyp( &x, &y, &p );
giet_tty_printf("\n[DEBUG USER BARRIER] proc[%d,%d,%d] enters barrier_wait()\n", 
                x, y, p );
#endif

    // compute expected sense value 
    unsigned int expected;
    if ( barrier->sense == 0 ) expected = 1;
    else                       expected = 0;

    // parallel decrement barrier counter using atomic instructions LL/SC
    // - input : pointer on the barrier counter (pcount)
    // - output : counter value (count)
    volatile unsigned int* pcount  = (unsigned int *)&barrier->count;
    volatile unsigned int  count    = 0;  // avoid a warning

    asm volatile( "addu   $2,     %1,        $0      \n"
                  "barrier_llsc:                     \n"
                  "ll     $8,     0($2)              \n"
                  "addi   $9,     $8,        -1      \n"
                  "sc     $9,     0($2)              \n"
                  "beqz   $9,     barrier_llsc       \n"
                  "addu   %0,     $8,        $0      \n"
                  : "=r" (count)
                  : "r" (pcount)
                  : "$2", "$8", "$9", "memory" );

    // the last task re-initializes count and toggle sense,
    // waking up all other waiting tasks
    if (count == 1)   // last task
    {
        barrier->count = barrier->arity;
        barrier->sense = expected;
    }
    else              // other tasks busy waiting the sense flag
    {
        // polling sense flag
        // input: pointer on the sens flag (psense)
        // input: expected sense value (expected)
        unsigned int* psense  = (unsigned int *)&barrier->sense;
        asm volatile ( "barrier_sense:                   \n"
                       "lw    $3,   0(%0)                \n"
                       "bne   $3,   %1,    barrier_sense \n"
                       :
                       : "r"(psense), "r"(expected)
                       : "$3" );
    }

    asm volatile ("sync" ::: "memory");

#if GIET_DEBUG_USER_BARRIER
giet_tty_printf("\n[DEBUG USER BARRIER] proc[%d,%d,%d] exit barrier_wait()\n",
                x, y, p );
#endif

}

///////////////////////////////////////////////////////////////////////////////////
//      SQT barrier access functions
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////
static
void sqt_barrier_build( giet_sqt_barrier_t*  barrier, 
                        unsigned int         x,
                        unsigned int         y,
                        unsigned int         level,
                        sqt_node_t*          parent,
                        unsigned int         x_size,
                        unsigned int         y_size,
                        unsigned int         ntasks ) 
{
    // This recursive function initializes the SQT nodes
    // traversing the SQT from root to bottom

    // get target node address
    sqt_node_t* node = barrier->node[x][y][level];
    
    if (level == 0 )        // terminal case
    {
        // initializes target node
        node->arity    = ntasks;   
        node->count    = ntasks;   
        node->sense    = 0;   
        node->level    = 0;   
        node->parent   = parent;
        node->child[0] = NULL;
        node->child[1] = NULL;
        node->child[2] = NULL;
        node->child[3] = NULL;

#if GIET_DEBUG_USER_BARRIER
giet_tty_printf("\n[DEBUG USER BARRIER] initialize sqt_node[%d][%d][%d] : arity = %d\n"
                " parent = %x / child0 = %x / child1 = %x / child2 = %x / child3 = %x\n", 
                x, y, level, node->arity, 
                (unsigned int)node->parent,
                (unsigned int)node->child[0],
                (unsigned int)node->child[1],
                (unsigned int)node->child[2],
                (unsigned int)node->child[3] );
#endif

    }
    else                   // non terminal case
    {
        unsigned int cx[4];   // x coordinate for children
        unsigned int cy[4];   // y coordinate for children
        unsigned int arity = 0;
        unsigned int i;

        // the child0 coordinates are equal to the parent coordinates
        // other children coordinates are incremented depending on the level value
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
            if ( (cx[i] < x_size) && (cy[i] < y_size) ) 
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

#if GIET_DEBUG_USER_BARRIER
giet_tty_printf("\n[DEBUG USER BARRIER] initialize sqt_node[%d][%d][%d] : arity = %d\n"
                " parent = %x / child0 = %x / child1 = %x / child0 = %x / child1 = %x\n", 
                x, y, level, node->arity, 
                (unsigned int)node->parent,
                (unsigned int)node->child[0],
                (unsigned int)node->child[1],
                (unsigned int)node->child[2],
                (unsigned int)node->child[3] );
#endif

        // recursive calls for children nodes
        for ( i = 0 ; i < 4 ; i++ )
        {
            if ( (cx[i] < x_size) && (cy[i] < y_size) ) 
                sqt_barrier_build( barrier, 
                                   cx[i], 
                                   cy[i], 
                                   level-1, 
                                   node, 
                                   x_size,
                                   y_size,
                                   ntasks );
        }
    }
}  // end sqt_barrier_build()

////////////////////////////////////////////////////
void sqt_barrier_init( giet_sqt_barrier_t*  barrier,
                       unsigned int         x_size,    // number of clusters in a row
                       unsigned int         y_size,    // number of clusters in a col
                       unsigned int         ntasks )   // tasks per clusters
{
    // check parameters
    if ( x_size > 16 ) giet_exit("SQT BARRIER ERROR : x_size too large");
    if ( y_size > 16 ) giet_exit("SQT BARRIER ERROR : y_size too large");
    if ( ntasks > 8  ) giet_exit("SQT BARRIER ERROR : ntasks too large");
    
    // compute SQT levels
    unsigned int levels; 
    unsigned int z = (x_size > y_size) ? x_size : y_size;
    levels = (z < 2) ? 1 : (z < 3) ? 2 : (z < 5) ? 3 : (z < 9) ? 4 : 5;

#if GIET_DEBUG_USER_BARRIER
    unsigned int side; 
    side   = (z < 2) ? 1 : (z < 3) ? 2 : (z < 5) ? 4 : (z < 9) ? 8 : 16;
giet_tty_printf("\n[DEBUG USER BARRIER] sqt_nodes allocation\n"
                " x_size = %d / y_size = %d / levels = %d / side = %d\n",
                x_size , y_size , levels , side );
#endif

    // allocates memory for the SQT nodes and initializes SQT nodes pointers array
    // the number of SQT nodes in a cluster(x,y) depends on (x,y): 
    // At least 1 node / at most 5 nodes
    unsigned int x;          // x coordinate for one SQT node
    unsigned int y;          // y coordinate for one SQT node
    unsigned int l;          // level for one SQT node
    for ( x = 0 ; x < x_size ; x++ )
    {
        for ( y = 0 ; y < y_size ; y++ )
        {
            for ( l = 0 ; l < levels ; l++ )         
            {
                
                if ( ( (l == 0) && ((x&0x00) == 0) && ((y&0x00) == 0) ) ||
                     ( (l == 1) && ((x&0x01) == 0) && ((y&0x01) == 0) ) ||
                     ( (l == 2) && ((x&0x03) == 0) && ((y&0x03) == 0) ) ||
                     ( (l == 3) && ((x&0x07) == 0) && ((y&0x07) == 0) ) ||
                     ( (l == 4) && ((x&0x0F) == 0) && ((y&0x0F) == 0) ) )
                 {
                     barrier->node[x][y][l] = remote_malloc( sizeof(sqt_node_t), 
                                                             x, y );

#if GIET_DEBUG_USER_BARRIER
giet_tty_printf("\n[DEBUG USER BARRIER] SQT node[%d][%d][%d] : vaddr = %x\n",
                x, y, l, (unsigned int)barrier->node[x][y][l] );
#endif
                 }
            }
        }
    }
            
    // recursively initialize all SQT nodes from root to bottom
    sqt_barrier_build( barrier,
                       0,        
                       0,
                       levels-1,
                       NULL,
                       x_size,
                       y_size,
                       ntasks );

    asm volatile ("sync" ::: "memory");

}  // end sqt_barrier_init


 
///////////////////////////////////////
static
void sqt_barrier_decrement( sqt_node_t* node )
{
    // This recursive function decrements the distributed "count" variables,
    // traversing the SQT from bottom to root.
    // The last arrived task reset the local node before returning.

#if GIET_DEBUG_USER_BARRIER
unsigned int    px;
unsigned int    py;
unsigned int    pl;
giet_proc_xyp( &px, &py, &pl );
giet_tty_printf("\n[DEBUG USER BARRIER] P[%d,%d,%d] decrement SQT barrier node %x :\n"
                " level = %d / parent = %x / arity = %d / sense = %d / count = %d\n",
                px , py , pl , (unsigned int)node , 
                node->level , node->parent, node->arity , node->sense , node->count );
#endif

    // compute expected sense value 
    unsigned int expected;
    
    if ( node->sense == 0) expected = 1;
    else                   expected = 0;

    // atomic decrement
    // - input  : count address (pcount)
    // - output : modified counter value (count)
    unsigned int*  pcount    = (unsigned int*)&node->count;
    unsigned int   count     = 0;  // avoid a warning

    asm volatile( "addu   $2,     %1,        $0      \n"
                  "1234:                             \n"
                  "ll     $8,     0($2)              \n"
                  "addi   $9,     $8,        -1      \n"
                  "sc     $9,     0($2)              \n"
                  "beqz   $9,     1234b              \n"
                  "addu   %0,     $8,        $0      \n"
                  : "=r" (count)
                  : "r" (pcount)
                  : "$2", "$8", "$9", "memory" );
    
    if ( count == 1 )    // last task  
    {
        // decrement the parent node if the current node is not the root
        if ( node->parent != NULL )      {
            sqt_barrier_decrement( node->parent );
        }

        // reset the current node
        node->sense = expected;
        node->count = node->arity;

#if GIET_DEBUG_USER_BARRIER
giet_tty_printf("\n[DEBUG USER BARRIER] P[%d,%d,%d] reset SQT barrier node %x :\n"
                " level = %d / arity = %d / sense = %d / count = %d\n",
                px , py , pl , (unsigned int)node , 
                node->level , node->arity , node->sense , node->count );
#endif
        return;
    }
    else                 // not the last task
    {
        // poll sense flag
        // input: pointer on the sens flag (psense)
        // input: expected sense value (expected)
        unsigned int* psense  = (unsigned int *)&node->sense;
        asm volatile ( "5678:                            \n"
                       "lw    $3,   0(%0)                \n"
                       "bne   $3,   %1,    5678b         \n"
                       :
                       : "r"(psense), "r"(expected)
                       : "$3" );
        return;
    }
} // end sqt_decrement()
    
////////////////////////////////////////////////////
void sqt_barrier_wait( giet_sqt_barrier_t* barrier )
{
    // compute cluster coordinates for the calling task
    unsigned int    x;
    unsigned int    y;
    unsigned int    lpid;
    giet_proc_xyp( &x, &y, &lpid );
#if GIET_DEBUG_USER_BARRIER
giet_tty_printf("[DEBUG SQT USER BARRIER] proc[%d,%d,%d] enters sqt_barrier_wait(). vaddr=%x. node=%x\n", 
                x, y, lpid, barrier, barrier->node[x][y][0] );
#endif

    // recursively decrement count from bottom to root
    sqt_barrier_decrement( barrier->node[x][y][0] );

    asm volatile ("sync" ::: "memory");

}  // end sqt_barrier_wait()


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabsroot=4:softtabsroot=4

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

