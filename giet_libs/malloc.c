////////////////////////////////////////////////////////////////////////////////
// File     : malloc.c
// Date     : 05/03/2013
// Author   : Jean-Baptiste Bréjon / alain greiner
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////

#include "malloc.h"
#include "stdio.h"
#include "giet_config.h"

// Global variables defining the heap descriptors array (one heap per cluster)
giet_heap_t     heap[X_SIZE][Y_SIZE];

// Macro returning the smallest power of 2 larger or equal to size value
#define GET_SIZE_INDEX(size)                (size <= 0x00000001) ? 0  :\
                                            (size <= 0x00000002) ? 1  :\
                                            (size <= 0x00000004) ? 2  :\
                                            (size <= 0x00000008) ? 3  :\
                                            (size <= 0x00000010) ? 4  :\
                                            (size <= 0x00000020) ? 5  :\
                                            (size <= 0x00000040) ? 6  :\
                                            (size <= 0x00000080) ? 7  :\
                                            (size <= 0x00000100) ? 8  :\
                                            (size <= 0x00000200) ? 9  :\
                                            (size <= 0x00000400) ? 10 :\
                                            (size <= 0x00000800) ? 11 :\
                                            (size <= 0x00001000) ? 12 :\
                                            (size <= 0x00002000) ? 13 :\
                                            (size <= 0x00004000) ? 14 :\
                                            (size <= 0x00008000) ? 15 :\
                                            (size <= 0x00010000) ? 16 :\
                                            (size <= 0x00020000) ? 17 :\
                                            (size <= 0x00040000) ? 18 :\
                                            (size <= 0x00080000) ? 19 :\
                                            (size <= 0x00100000) ? 20 :\
                                            (size <= 0x00200000) ? 21 :\
                                            (size <= 0x00400000) ? 22 :\
                                            (size <= 0x00800000) ? 23 :\
                                            (size <= 0x01000000) ? 24 :\
                                            (size <= 0x02000000) ? 25 :\
                                            (size <= 0x04000000) ? 26 :\
                                            (size <= 0x08000000) ? 27 :\
                                            (size <= 0x10000000) ? 28 :\
                                            (size <= 0x20000000) ? 29 :\
                                            (size <= 0x40000000) ? 30 :\
                                            (size <= 0x80000000) ? 31 :\
                                                                   32
////////////////////////////////////////
void display_free_array( unsigned int x,
                         unsigned int y )
{
    unsigned int next;
    unsigned int id;
    unsigned int iter;

    giet_tty_printf("\nUser Heap[%d][%d] base = %x / size = %x\n", x , y , 
                   heap[x][y].heap_base, heap[x][y].heap_size );
    for ( id = 0 ; id < 32 ; id++ )
    { 
        next = heap[x][y].free[id];
        giet_tty_printf(" - free[%d] = " , id );
        iter = 0;
        while ( next != 0 )
        {
            giet_tty_printf("%x | ", next );
            next = (*(unsigned int*)next);
            iter++;
        }
        giet_tty_printf("0\n");
    }
}  // end display_free_array()



////////////////////////////////
void heap_init( unsigned int x,
                unsigned int y )
{
    unsigned int heap_base;        // heap segment base
    unsigned int heap_size;        // heap segment size
    unsigned int heap_index;       // size_index in free[array]

    unsigned int alloc_base;       // alloc[] array base 
    unsigned int alloc_size;       // alloc[] array size
    unsigned int alloc_index;      // size_index in alloc[array]

    unsigned int index;            // iterator

    // get heap_base, heap size, and heap index
    giet_heap_info( &heap_base, &heap_size, x, y );
    heap_index = GET_SIZE_INDEX( heap_size );

#if GIET_DEBUG_USER_MALLOC
giet_tty_printf("\n[DEBUG USER_MALLOC] Starting Heap[%d][%d] initialisation /"
                " base = %x / size = %x\n", x, y, heap_base, heap_size );
#endif

    // checking heap segment constraints
    if ( heap_size == 0 )                                    // heap segment exist
    {
        giet_exit("ERROR in malloc() : heap not found \n");
    }
    if ( heap_size != (1<<heap_index) )                      // heap size power of 2
    {
        giet_exit("ERROR in malloc() : heap size must be power of 2\n");
    }
    if ( heap_base % heap_size )                             // heap segment aligned
    {
        giet_exit("ERROR in malloc() : heap segment must be aligned\n");
    }

    // compute size of block containin alloc[] array 
    alloc_size = heap_size / MIN_BLOCK_SIZE;
    if ( alloc_size < MIN_BLOCK_SIZE) alloc_size = MIN_BLOCK_SIZE;

    // get index for the corresponding block
    alloc_index = GET_SIZE_INDEX( alloc_size );

    // compute alloc[] array base address
    alloc_base = heap_base + heap_size - alloc_size;

    // reset the free[] array 
    for ( index = 0 ; index < 32 ; index++ )
    {
        heap[x][y].free[index] = 0;
    }

    // reset the alloc_size array
    unsigned int   word;
    unsigned int*  tab = (unsigned int*)alloc_base;
    for ( word = 0 ; word < (alloc_size>>2) ; word++ )  tab[word] = 0;
 
    // split the heap into various sizes blocks,
    // initializes the free[] array and NEXT pointers
    // base is the block base address
    unsigned int   base = heap_base;
    unsigned int*  ptr;
    for ( index = heap_index-1 ; index >= alloc_index ; index-- )
    {
        heap[x][y].free[index] = base;
        ptr = (unsigned int*)base;
        *ptr = 0;
        base = base + (1<<index);
    }

    heap[x][y].init       = HEAP_INITIALIZED;
    heap[x][y].x          = x;
    heap[x][y].y          = y;
    heap[x][y].heap_base  = heap_base;
    heap[x][y].heap_size  = heap_size;
    heap[x][y].alloc_size = alloc_size;
    heap[x][y].alloc_base = alloc_base;

    lock_init( &heap[x][y].lock );

#if GIET_DEBUG_USER_MALLOC
giet_tty_printf("\n[DEBUG USER_MALLOC] Completing Heap[%d][%d] initialisation\n", x, y );
display_free_array(x,y);
#endif
                
}  // end heap_init()

////////////////////////////////////////////
unsigned int split_block( giet_heap_t* heap,
                          unsigned int vaddr, 
                          unsigned int searched_index,
                          unsigned int requested_index )
{
    // push the upper half block into free[searched_index-1]
    unsigned int* new            = (unsigned int*)(vaddr + (1<<(searched_index-1)));
    *new                         = heap->free[searched_index-1]; 
    heap->free[searched_index-1] = (unsigned int)new;
        
    if ( searched_index == requested_index + 1 )  // terminal case: return lower half block 
    {
        return vaddr;
    }
    else            // non terminal case : lower half block must be split again
    {                               
        return split_block( heap, vaddr, searched_index-1, requested_index );
    }
} // end split_block()

//////////////////////////////////////////
unsigned int get_block( giet_heap_t* heap,
                        unsigned int searched_index,
                        unsigned int requested_index )
{
    // test terminal case
    if ( (1<<searched_index) > heap->heap_size )  // failure : return a NULL value
    {
        return 0;
    }
    else                            // search a block in free[searched_index]
    {
        unsigned int vaddr = heap->free[searched_index];
        if ( vaddr == 0 )     // block not found : search in free[searched_index+1]
        {
            return get_block( heap, searched_index+1, requested_index );
        }
        else                // block found : pop it from free[searched_index] 
        {
            // pop the block from free[searched_index]
            unsigned int next = *((unsigned int*)vaddr); 
            heap->free[searched_index] = next;
            
            // test if the block must be split
            if ( searched_index == requested_index )  // no split required
            {
                return vaddr;
            }
            else                                      // split is required
            {
                return split_block( heap, vaddr, searched_index, requested_index );
            }
        } 
    }
} // end get_block()

////////////////////////////////////////
void * remote_malloc( unsigned int size,
                      unsigned int x,
                      unsigned int y ) 
{

#if GIET_DEBUG_USER_MALLOC
giet_tty_printf("\n[DEBUG USER_MALLOC] request for Heap[%d][%d] / size = %x\n", 
                 x, y, size );
#endif

    // checking arguments
    if (size == 0) 
    {
        giet_exit("\nERROR in remote_malloc() : requested size = 0 \n");
    }
    if ( x >= X_SIZE )
    {
        giet_exit("\nERROR in remote_malloc() : x coordinate too large\n");
    }
    if ( y >= Y_SIZE )
    {
        giet_exit("\nERROR in remote_malloc() : y coordinate too large\n");
    }

    // checking initialization
    if ( heap[x][y].init != HEAP_INITIALIZED )
    {
        giet_exit("\nERROR in remote_malloc() : heap not initialized\n");
    }

    // normalize size
    if ( size < MIN_BLOCK_SIZE ) size = MIN_BLOCK_SIZE;

    // compute requested_index for the free[] array
    unsigned int requested_index = GET_SIZE_INDEX( size );

    // take the lock protecting access to heap[x][y]
    lock_acquire( &heap[x][y].lock );

    // call the recursive function get_block
    unsigned int base = get_block( &heap[x][y], 
                                   requested_index, 
                                   requested_index );

    // check block found
    if ( base == 0 )
    {
        lock_release( &heap[x][y].lock );
        giet_exit("\nERROR in remote_malloc() : no more space\n");
    }

    // compute pointer in alloc[] array
    unsigned offset    = (base - heap[x][y].heap_base) / MIN_BLOCK_SIZE;
    unsigned char* ptr = (unsigned char*)(heap[x][y].alloc_base + offset);

    // check the alloc[] array
    if ( *ptr != 0 )
    {
        lock_release( &heap[x][y].lock );
        giet_exit("\nERROR in remote_malloc() : block already allocated ???\n");
    }

    // update alloc_array
    *ptr = requested_index;

    // release the lock
    lock_release( &heap[x][y].lock );
 
#if GIET_DEBUG_USER_MALLOC
giet_tty_printf("\n[DEBUG USER_MALLOC] allocated block from heap[%d][%d] : "
                "base = %x / size = %x\n", x , y , base , size );
display_free_array(x,y);
#endif

    return (void*)base;

} // end remote_malloc()


//////////////////////////////////
void * malloc( unsigned int size )
{
    // get cluster coordinates
    unsigned int    x;
    unsigned int    y;
    unsigned int    lpid;
    giet_proc_xyp( &x, &y, &lpid );

    return remote_malloc( size, x, y );
} 

///////////////////////////////////////////
void update_free_array( giet_heap_t* heap,
                        unsigned int base,
                        unsigned int size_index )
{
    // This recursive function try to merge the released block 
    // with the companion block if this companion block is free.
    // This companion has the same size, and almost the same address
    // (only one address bit is different)
    // - If the companion is not in free[size_index],
    //   the released block is pushed in free[size_index].
    // - If the companion is found, it is evicted from free[size_index]
    //   and the merged bloc is pushed in the free[size_index+1].


    // compute released block size
    unsigned int size = 1<<size_index;

    // compute companion block and merged block base addresses
    unsigned int companion_base;  
    unsigned int merged_base;  

    if ( (base & size) == 0 )   // the released block is aligned on (2*size)
    {
        companion_base  = base + size;
        merged_base     = base;
    }
    else
    {
        companion_base  = base - size;
        merged_base     = base - size;
    }

    // scan all blocks in free[size_index]
    // the iter & prev variables are actually addresses
    unsigned int  found = 0;
    unsigned int  iter  = heap->free[size_index];
    unsigned int  prev  = (unsigned int)&heap->free[size_index];
    while ( iter ) 
    {
        if ( iter == companion_base ) 
        {
            found = 1;
            break;
        }
        prev = iter;
        iter = *(unsigned int*)iter;
    }

    if ( found == 0 )  // Companion not found => push in free[size_index]  
    {
        *(unsigned int*)base   = heap->free[size_index];
        heap->free[size_index] = base;
    }
    else               // Companion found : merge
    {
        // evict the searched block from free[size_index]
        *(unsigned int*)prev = *(unsigned int*)iter;

        // call the update_free() function for free[size_index+1]
        update_free_array( heap, merged_base , size_index+1 );
    }
}

//////////////////////
void free( void* ptr )
{
    // get the cluster coordinate from ptr value
    unsigned int x;
    unsigned int y;
    giet_get_xy( ptr, &x, &y );

#if GIET_DEBUG_USER_MALLOC
giet_tty_printf("\n[DEBUG USER_MALLOC] Free for vaddr = %x / x = %d / y = %d\n",
                 (unsigned int)ptr, x, y );
#endif

    // check ptr value
    unsigned int base = (unsigned int)ptr;
    if ( (base < heap[x][y].heap_base) || 
         (base >= (heap[x][y].heap_base + heap[x][y].heap_size)) )
    {
        giet_exit("ERROR in free() : illegal pointer for released block");
    }
 
    // get the lock protecting heap[x][y]
    lock_acquire( &heap[x][y].lock );

    // compute released block index in alloc[] array
    unsigned index = (base - heap[x][y].heap_base ) / MIN_BLOCK_SIZE;
 
    // get the released block size_index
    unsigned char* pchar      = (unsigned char*)(heap[x][y].alloc_base + index);
    unsigned int   size_index = (unsigned int)*pchar;

    // check block allocation
    if ( size_index == 0 )
    {
        lock_release( &heap[x][y].lock );
        giet_exit("\nERROR in free() : released block not allocated ???\n");
    }

    // check released block alignment
    if ( base % (1 << size_index) )
    {
        giet_exit("\nERROR in free() : released block not aligned\n");
    }

    // call the recursive function update_free_array() 
    update_free_array( &heap[x][y], base, size_index ); 

    // release the lock
    lock_release( &heap[x][y].lock );

#if GIET_DEBUG_USER_MALLOC
display_free_array(x,y);
#endif

} // end free()

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4



