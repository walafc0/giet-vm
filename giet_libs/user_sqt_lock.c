
#include "stdio.h"
#include "malloc.h"
#include "giet_config.h"
#include "user_lock.h"
#include "user_sqt_lock.h"

static 
void sqt_lock_build( sqt_lock_t*      lock,      // pointer on the SQT lock
                      unsigned int     x,         // node X coordinate
                      unsigned int     y,         // node Y coordinate
                      unsigned int     level,     // node level
                      sqt_lock_node_t* parent,    // pointer on parent node
                      unsigned int     xmax,      // SQT X size
                      unsigned int     ymax )     // SQT Y size
{

#if GIET_DEBUG_USER_LOCK

unsigned int    px;
unsigned int    py;
unsigned int    pl;
giet_proc_xyp( &px, &py, &pl );
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

#if GIET_DEBUG_USER_LOCK
giet_tty_printf("\n[DEBUG USER SQT_LOCK] P[%d,%d,%d] initialises SQT node[%d,%d,%d] : \n"
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

#if GIET_DEBUG_USER_LOCK
giet_tty_printf("\n[DEBUG USER SQT_LOCK] P[%d,%d,%d] initialises SQT node[%d,%d,%d] : \n"
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
                sqt_lock_build( lock, 
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
void sqt_lock_init( sqt_lock_t*  lock,
                       unsigned int         x_size,    // number of clusters in a row
                       unsigned int         y_size,    // number of clusters in a col
                       unsigned int         ntasks )   // tasks per clusters
{
    // check parameters
    if ( x_size > 16 ) giet_exit("SQT LOCK ERROR : x_size too large");
    if ( y_size > 16 ) giet_exit("SQT LOCK ERROR : y_size too large");
    if ( ntasks > 8  ) giet_exit("SQT LOCK ERROR : ntasks too large");
    
    // compute SQT levels
    unsigned int levels; 
    unsigned int z = (x_size > y_size) ? x_size : y_size;
    levels = (z < 2) ? 1 : (z < 3) ? 2 : (z < 5) ? 3 : (z < 9) ? 4 : 5;

#if GIET_DEBUG_USER_LOCK
unsigned int    px;
unsigned int    py;
unsigned int    pl;
giet_proc_xyp(&px, &py, &pl);
unsigned int side   = (z < 2) ? 1 : (z < 3) ? 2 : (z < 5) ? 4 : (z < 9) ? 8 : 16;
giet_tty_printf("\n[DEBUG USER SQT_LOCK] sqt_nodes allocation\n"
                " x_size = %d / y_size = %d / levels = %d / side = %d\n",
                x_size , y_size , levels , side );
#endif

    
    unsigned int x;              // x coordinate for one SQT node
    unsigned int y;              // y coordinate for one SQT node
    unsigned int l;              // level for one SQT node

    for ( x = 0 ; x < x_size ; x++ )
    {
        for ( y = 0 ; y < y_size ; y++ )
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
                     (sqt_lock_node_t*)remote_malloc( sizeof(sqt_lock_node_t),
                                                       x, y );

#if GIET_DEBUG_USER_LOCK
giet_tty_printf("\n[DEBUG USER SQT_LOCK] P[%d,%d,%d] allocates SQT node[%d,%d,%d]"
               " : vaddr = %x\n",
               px , py , pl , x , y , l , (unsigned int)lock->node[x][y][l] );
#endif
                 }
            }
        }
    }
            
    // recursively initialize all SQT nodes from root to bottom
    sqt_lock_build( lock,
                     0,
                     0,
                     levels-1,
                     NULL,
                     x_size,
                     y_size );

    asm volatile ("sync" ::: "memory");

#if GIET_DEBUG_USER_LOCK
giet_tty_printf("\n[DEBUG USER SQT_LOCK] SQT nodes initialisation completed\n"); 
#endif

} // end sqt_lock_init()


//////////////////////////////////////////////////////////////////////////////////
static 
void sqt_lock_take( sqt_lock_node_t* node )
{
    // get next free ticket from local lock
    unsigned int ticket = atomic_increment( &node->free, 1 );

#if GIET_DEBUG_USER_LOCK
unsigned int    x;
unsigned int    y;
unsigned int    l;
giet_proc_xyp(&x, &y, &l);
giet_tty_printf("\n[DEBUG USER SQT_LOCK] P[%d,%d,%d] get ticket %d for SQT lock %x"
               " / level = %d / current = %d / free = %d\n",
               x , y , l , ticket , (unsigned int)node ,
               node->level , node->current , node->free );
#endif

    // poll the local lock current index
    while ( (*(volatile unsigned int *)( &node->current )) != ticket ) asm volatile( "nop" );

#if GIET_DEBUG_USER_LOCK
giet_tty_printf("\n[DEBUG SQT_LOCK] P[%d,%d,%d] get SQT lock %x"
               " / level = %d / current = %d / free = %d\n",
               x , y , l , (unsigned int)node ,
               node->level , node->current , node->free );
#endif

    // try to take the parent node lock until top is reached
    if ( node->parent != NULL ) sqt_lock_take( node->parent );

} // end _sqt_lock_take()


/////////////////////////////////////////////////////////////////////////////////
void sqt_lock_acquire( sqt_lock_t*  lock )
{
    unsigned int x;
    unsigned int y;
    unsigned int p;
    // get cluster coordinates
    giet_proc_xyp( &x, &y, &p );

    // try to recursively take the distributed locks (from bottom to top)
    sqt_lock_take( lock->node[x][y][0] );
}

/////////////////////////////////////////////////////////////////////////////////
static 
void sqt_lock_give( sqt_lock_node_t* node )
{
    // release the local lock
    node->current = node->current + 1;

#if GIET_DEBUG_USER_LOCK
unsigned int    x;
unsigned int    y;
unsigned int    l;
giet_proc_xyp(&x, &y, &l);
giet_tty_printf("\n[DEBUG USER SQT_LOCK] P[%d,%d,%d] release SQT lock %x"
               " / level = %d / current = %d / free = %d\n",
               x , y , l , (unsigned int)node, 
               node->level , node->current , node->free );
#endif

    // reset parent node until top is reached
    if ( node->parent != NULL ) sqt_lock_give( node->parent );

} // end _sqt_lock_give()

/////////////////////////////////////////////////////////////////////////////////
void sqt_lock_release( sqt_lock_t*  lock )
{
    asm volatile ( "sync" );   // for consistency

    unsigned int x;
    unsigned int y;
    unsigned int p;
    // get cluster coordinates
    giet_proc_xyp( &x, &y, &p );

    // recursively reset the distributed locks (from bottom to top)
    sqt_lock_give( lock->node[x][y][0] );
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

