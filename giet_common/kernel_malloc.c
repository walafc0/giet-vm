////////////////////////////////////////////////////////////////////////////////
// File     : kernel_malloc.c
// Date     : 05/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
// - As this code is used to implement the SQT lock ptotecting TTY0,
//   all functions here use the kernel _nolock_printf() function.
// - It must exist one vseg with the HEAP type in each cluster. The length
//   must be a power of 2, and the base address must be aligned.
// - An array kernel_heap[x][y] containing the heap descriptors is
//   stored in cluster[0][0].
// - Each kernel_heap[x][y] structure contains a specific queuing spin-lock.
////////////////////////////////////////////////////////////////////////////////
// Allocation policy:
// - All allocated blocks have a size that is a power of 2, larger or equal
//   to MIN_BLOCK_SIZE (typically 64 bytes), and are aligned.
// - All free blocks are pre-classed in 32 linked lists of free blocks, where 
//   all blocks in a given list have the same size. 
// - The NEXT pointer implementing those linked lists is written 
//   in the 4 first bytes of the block itself, using the unsigned int type.
// - The pointers on the first free block for each size are stored in an
//   array of pointers free[32] in the heap[x][y) structure itself.
// - The block size required can be any value, but the allocated block size
//   is the smallest power of 2 value larger or equal to the requested size.
// - It pop the linked list of free blocks corresponding to size,
//   and returns the block B if the list[size] is not empty.
// - If the list[size] is empty, it pop the list[size * 2].
//   If a block B' is found, it break this block in 2 B/2 blocks, returns 
//   the first B/2 block and push the other B/2 block into list[size].
// - If the list[size * 2] is empty, it pop the list[size * 4].
//   If a block B is found, it break this block in 3 blocks B/4, B/4 and B/2,
//   returns the first B/4 block, push the other blocks B/4 and B/2 into
//   the proper lists. etc... 
////////////////////////////////////////////////////////////////////////////////
// Free policy:
// - Each allocated block is registered in an alloc[] array of unsigned char.
// - This registration is required by the free() operation, because the size
//   of the allocated block must be obtained from the base address of the block.  
// - The number of entries in this array is equal to the max number
//   of allocated block is : heap_size / MIN_BLOCK_SIZE
// - For each allocated block, the value registered in the alloc[] array
//   is log2( size_of_allocated_block ).
// - The index in this array is computed from the allocated block base address:
//      index = (block_base - heap_base) / MIN_BLOCK_SIZE
// - The alloc[] array is stored at the end of heap segment. This consume
//   (1 / MIN_BLOCK_SIZE) of the total heap storage capacity.
////////////////////////////////////////////////////////////////////////////////

#include "giet_config.h"
#include "hard_config.h"
#include "mapping_info.h"
#include "kernel_malloc.h"
#include "kernel_locks.h"
#include "sys_handler.h"
#include "tty0.h"
#include "utils.h"

///////////////////////////////////////////////////////////////////////////////
// Global variables defining the heap descriptors array (one heap per cluster)
///////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
kernel_heap_t     kernel_heap[X_SIZE][Y_SIZE];

///////////////////////////////////////////////////////////////////////////////
// Macro returning the smallest power of 2 larger or equal to size value
///////////////////////////////////////////////////////////////////////////////
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

#if GIET_DEBUG_SYS_MALLOC

////////////////////////////////////////////////
static void _display_free_array( unsigned int x,
                                 unsigned int y )
{
    unsigned int next;
    unsigned int id;
    unsigned int iter;

    _nolock_printf("\nKernel Heap[%d][%d] base = %x / size = %x\n", x , y , 
                   kernel_heap[x][y].heap_base, kernel_heap[x][y].heap_size );
    for ( id = 6 ; id < 24 ; id++ )
    { 
        next = kernel_heap[x][y].free[id];
        _nolock_printf(" - free[%d] = " , id );
        iter = 0;
        while ( next != 0 )
        {
            _nolock_printf("%x | ", next );
            next = (*(unsigned int*)next);
            iter++;
        }
        _nolock_printf("0\n");
    }
}  // end _display_free_array()

#endif




////////////////////////////////////////////////////////////////////////////////
// This function returns the heap_base and heap_size values for cluster[x][y],
// from information defined in the mapping.
////////////////////////////////////////////////////////////////////////////////
unsigned int _get_heap_info( unsigned int* heap_base,
                             unsigned int* heap_size,
                             unsigned int  x,
                             unsigned int  y )
{
    mapping_header_t  * header   = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vseg_t    * vsegs    = _get_vseg_base(header);
    mapping_pseg_t    * psegs    = _get_pseg_base(header);
    mapping_cluster_t * clusters = _get_cluster_base(header);

    unsigned int vseg_id;
    unsigned int pseg_id;
    unsigned int cluster_id;

    // checking coordinates
    if ( (x >= X_SIZE) || (y >= Y_SIZE) )
    {
        _nolock_printf("\n[GIET ERROR] _get_heap_info()"
                       " illegal (%d,%d) coordinates\n", x , y );
        _exit();
    }

    // scan all global vsegs
    for ( vseg_id = 0 ; vseg_id < header->globals ; vseg_id++ )
    {
        pseg_id    = vsegs[vseg_id].psegid;
        cluster_id = psegs[pseg_id].clusterid;
        if ( (vsegs[vseg_id].type == VSEG_TYPE_HEAP) &&
             (clusters[cluster_id].x == x) && 
             (clusters[cluster_id].y == y) )
        {
            *heap_base = vsegs[vseg_id].vbase;
            *heap_size = vsegs[vseg_id].length;
            return 0;
        }
    }

    return 1;
} // end _get_heap_info()



/////////////////
void _heap_init()
{
    unsigned int heap_base;
    unsigned int heap_size;
    unsigned int heap_index;

    unsigned int alloc_base; 
    unsigned int alloc_size;
    unsigned int alloc_index;

    unsigned int index;
    unsigned int x;
    unsigned int y;
    unsigned int ko;

    for ( x = 0 ; x < X_SIZE ; x++ )
    {
        for ( y = 0 ; y < Y_SIZE ; y++ )
        {
            // get heap_base & heap size
            ko = _get_heap_info( &heap_base, &heap_size, x, y );
       
            if ( ko )  // no kernel heap found in cluster[x][y]
            {
                // initialise kernel_heap[x][y] descriptor to empty
                kernel_heap[x][y].heap_base  = 0;
                kernel_heap[x][y].heap_size  = 0;
            }
            else       // kernel heap found in cluster[x][y]
            {
                heap_index = GET_SIZE_INDEX( heap_size );

                // check heap[x][y] constraints
                if ( heap_size != (1<<heap_index) )
                {
                    _nolock_printf("\n[GIET ERROR] in _heap_init()"
                                   " kernel_heap[‰d,‰d] not power of 2\n", x , y );
                    _exit();
                }
                if ( heap_base % heap_size ) 
                {
                    _nolock_printf("\n[GIET ERROR] in _heap_init()"
                                   " kernel_heap[‰d,‰d] not aligned\n", x , y );
                    _exit();
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
                    kernel_heap[x][y].free[index] = 0;
                }

                // reset the alloc_size array
                memset( (unsigned char*)alloc_base , 0 , alloc_size );
 
                // split the heap into various sizes blocks,
                // initializes the free[] array and NEXT pointers
                // base is the block base address
                unsigned int   base = heap_base;
                unsigned int*  ptr;
                for ( index = heap_index-1 ; index >= alloc_index ; index-- )
                {
                    kernel_heap[x][y].free[index] = base;
                    ptr = (unsigned int*)base;
                    *ptr = 0;
                    base = base + (1<<index);
                }

                kernel_heap[x][y].heap_base  = heap_base;
                kernel_heap[x][y].heap_size  = heap_size;
                kernel_heap[x][y].alloc_size = alloc_size;
                kernel_heap[x][y].alloc_base = alloc_base;

                // initialise lock
                _spin_lock_init( &kernel_heap[x][y].lock );
            }

#if GIET_DEBUG_SYS_MALLOC
_nolock_printf("\n[DEBUG KERNEL_MALLOC] Completing kernel_heap[%d][%d]"
               " initialisation\n", x, y );
_display_free_array(x,y);
#endif
                
        }
    }
}  // end _heap_init()



//////////////////////////////////////////////
unsigned int _split_block( kernel_heap_t* heap,
                           unsigned int   vaddr, 
                           unsigned int   searched_index,
                           unsigned int   requested_index )
{
    // push the upper half block into free[searched_index-1]
    unsigned int* new            = (unsigned int*)(vaddr + (1<<(searched_index-1)));
    *new                         = heap->free[searched_index-1]; 
    heap->free[searched_index-1] = (unsigned int)new;
        
    if ( searched_index == requested_index + 1 )  //  return lower half block 
    {
        return vaddr;
    }
    else            // non terminal case : lower half block must be split again
    {                               
        return _split_block( heap, vaddr, searched_index-1, requested_index );
    }
} // end _split_block()



/////////////////////////////////////////////
unsigned int _get_block( kernel_heap_t* heap,
                         unsigned int   searched_index,
                         unsigned int   requested_index )
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
            return _get_block( heap, searched_index+1, requested_index );
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
                return _split_block( heap, vaddr, searched_index, requested_index );
            }
        } 
    }
} // end _get_block()



////////////////////////////////////////
void* _remote_malloc( unsigned int size,
                      unsigned int x,
                      unsigned int y ) 
{
    // checking arguments
    if ( x >= X_SIZE )
    {
        _nolock_printf("\n[GIET ERROR] _remote_malloc() : x coordinate too large\n");
        _exit();
    }
    if ( y >= Y_SIZE )
    {
        _nolock_printf("\n[GIET ERROR] _remote_malloc() : y coordinate too large\n");
        _exit();
    }
    if ( kernel_heap[x][y].heap_size == 0 )
    {
        _nolock_printf("\n[GIET ERROR] _remote_malloc() : No heap[%d][%d]\n", x, y );
        _exit();
     
    }

    // normalize size
    if ( size < MIN_BLOCK_SIZE ) size = MIN_BLOCK_SIZE;

    // compute requested_index for the free[] array
    unsigned int requested_index = GET_SIZE_INDEX( size );

    // get the lock protecting heap[x][y]
    _spin_lock_acquire( &kernel_heap[x][y].lock );

    // call the recursive function get_block()
    unsigned int base = _get_block( &kernel_heap[x][y], 
                                    requested_index, 
                                    requested_index );

    // check block found
    if ( base == 0 )
    {
        _nolock_printf("\n[GIET ERROR] in _remote_malloc() : "
                       "no more space in kernel_heap[%d][%d]\n", x , y );
        _spin_lock_release( &kernel_heap[x][y].lock );
        _exit();
    }

    // compute pointer in alloc[] array
    unsigned offset    = (base - kernel_heap[x][y].heap_base) / MIN_BLOCK_SIZE;
    unsigned char* ptr = (unsigned char*)(kernel_heap[x][y].alloc_base + offset);

    // check the alloc[] array
    if ( *ptr != 0 )
    {
        _nolock_printf("\n[GIET ERROR] in _remote_malloc() for heap[%d][%d] "
                       "selected block %X already allocated...\n", x , y , base );
        _spin_lock_release( &kernel_heap[x][y].lock );
        _exit();
    }

    // update alloc_array
    *ptr = requested_index;

    // release the lock
    _spin_lock_release( &kernel_heap[x][y].lock );
 
#if GIET_DEBUG_SYS_MALLOC
_nolock_printf("\n[DEBUG KERNEL_MALLOC] _remote-malloc()"
               " / vaddr %x / size = %x from heap[%d][%d]\n", base , size, x , y );
_display_free_array(x,y);
#endif

    return (void*)base;

} // end _remote_malloc()



//////////////////////////////////
void* _malloc( unsigned int size )
{
    unsigned int procid  = _get_procid();
    unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
    unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);

    return _remote_malloc( size , x , y );

}  // end _malloc()


///////////////////////////////////////////////
void _update_free_array( kernel_heap_t*  heap,
                         unsigned int    base,
                         unsigned int    size_index )
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

    // compute companion block and merged block base address 
    unsigned int companion_base; 
    unsigned int merged_base;  

    if ( (base & size) == 0 )   // the released block is aligned on (size*2)
    {
        companion_base  = base + size;
        merged_base     = base;
    }
    else
    {
        companion_base  = base - size;
        merged_base     = base - size;
    }

#if GIET_DEBUG_SYS_MALLOC > 1
_nolock_printf("\n[DEBUG KERNEL_MALLOC] _update_free_array() :\n"
               " - size           = %X\n"
               " - released_base  = %X\n"
               " - companion_base = %X\n"
               " - merged_base    = %X\n",
               size , base , companion_base , merged_base , (base & size) );
#endif

    // scan all blocks in free[size_index]
    // the iter & prev variables are actually addresses
    unsigned int  found = 0;
    unsigned int  iter  = heap->free[size_index];
    unsigned int  prev  = (unsigned int)(&heap->free[size_index]);
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

    if ( found == 0 )  // Companion not found => register in free[size_index]
    {

#if GIET_DEBUG_SYS_MALLOC > 1
_nolock_printf("\n[DEBUG KERNEL_MALLOC] _update_free_array() : companion "
               " not found => register block %x in free[%d]", base , size );
#endif

        // push the block in free[size_index]  
        *(unsigned int*)base   = heap->free[size_index];
        heap->free[size_index] = base;
    }
    else               // Companion found : merge and try in free[size_index + 1]
    {
        // pop the companion block (address == iter) from free[size_index]
        *(unsigned int*)prev = *(unsigned int*)iter;

        // call the update_free() function for free[size_index+1]
        _update_free_array( heap, merged_base , size_index+1 );
    }
}  // end _update_free_array()



///////////////////////
void _free( void* ptr )
{
    // get cluster coordinates from ptr value
    unsigned int x;
    unsigned int y;
    _sys_xy_from_ptr( ptr, &x, &y );

    // check ptr value
    unsigned int base = (unsigned int)ptr;
    if ( (base < kernel_heap[x][y].heap_base) || 
         (base >= (kernel_heap[x][y].heap_base + kernel_heap[x][y].heap_size)) )
    {
        _printf("\n[GIET ERROR] in _free() : illegal pointer for released block");
        _exit();
    }
 
    // get the lock protecting heap[x][y]
    _spin_lock_acquire( &kernel_heap[x][y].lock );

    // compute released block index in alloc[] array
    unsigned index = (base - kernel_heap[x][y].heap_base ) / MIN_BLOCK_SIZE;
 
    // get the released block size_index
    unsigned char* pchar      = (unsigned char*)(kernel_heap[x][y].alloc_base + index);
    unsigned int   size_index = (unsigned int)*pchar;

    // check block allocation
    if ( size_index == 0 )
    {
        _printf("\n[GIET ERROR] in _free() : released block %X not allocated "
                "in kernel_heap[%d][%d]\n", (unsigned int)ptr , x , y );
        _spin_lock_release( &kernel_heap[x][y].lock );
        _exit();
    }

    // check released block alignment
    if ( base % (1 << size_index) )
    {
        _printf("\n[GIET ERROR] in _free() : released block %X not aligned "
                "in kernel_heap[%d][%d]\n", (unsigned int)ptr , x , y );
        _spin_lock_release( &kernel_heap[x][y].lock );
        _exit();
    }

    // remove block from allocated blocks array
    *pchar = 0;

    // call the recursive function update_free_array() 
    _update_free_array( &kernel_heap[x][y] , base , size_index ); 

    // release the lock
    _spin_lock_release( &kernel_heap[x][y].lock );

#if GIET_DEBUG_SYS_MALLOC
_nolock_printf("\n[DEBUG KERNEL_MALLOC] _free() : vaddr = %x / size = %x "
               "to heap[%d][%d]\n", (unsigned int)ptr , 1<<size_index , x , y );
_display_free_array(x,y);
#endif

}  // end _free()

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4



