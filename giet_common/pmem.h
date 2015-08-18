///////////////////////////////////////////////////////////////////////////////////
// File     : pmem.h
// Date     : 01/09/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The pmem.c and pmem.h files are part ot the GIET-VM nano kernel.
// They define the data structures and functions used by the boot code
// to allocate physical memory. 
///////////////////////////////////////////////////////////////////////////////////
// The physical address format is 40 bits, structured in five fields:
//     | 4 | 4 |  11  |  9   |   12   |
//     | X | Y | BPPI | SPPI | OFFSET |
// - The (X,Y) fields define the cluster index
// - The BPPI field is the Big Physical Page Index
// - The SPPI field is the Small Physical Page Index
// - The |X|Y|BPPI|SPPI| concatenation is the PPN (Physical Page Number)
//
// The physical memory allocation is statically done by the boot-loader.
// As the physical memory can be distributed in all clusters, there is
// one physical memory allocator in each cluster, and the allocation
// state is defined by the boot_pmem_alloc[x][y] array (defined in the
// boot.c file).
// As the allocated physical memory is never released, the allocator structure
// is very simple and is defined below in the pmem_alloc_t structure.
// As the boot-loader is executed by one single processor, this structure 
// does not contain any lock protecting exclusive access.
// Both small pages allocator and big pages allocators allocate a variable
// number of CONTIGUOUS pages in the physical space.
// The first big page in cluster[0][0] is reserved for identity mapping vsegs,
// and is not allocated by the pmem allocator.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _PMEM_H_
#define _PMEM_H_

/////////////////////////////////////////////////////////////////////////////////////
// Physical memory allocator in cluster[x][y]
/////////////////////////////////////////////////////////////////////////////////////

typedef struct PmemAlloc 
{ 
    unsigned int x;          // allocator x coordinate
    unsigned int y;          // allocator y coordinate
    unsigned int max_bppi;   // max bppi value in cluster[x][y]
    unsigned int nxt_bppi;   // next free bppi in cluster[x][y]
    unsigned int max_sppi;   // max sppi value in cluster[x][y]
    unsigned int nxt_sppi;   // next free sppi in cluster[x][y]
    unsigned int spp_bppi;   // current bppi for small pages 
}  pmem_alloc_t;

////////////////////////////////////////////////////////////////////////////////////
// functions prototypes
////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// This function initialises the physical memory allocator in cluster (x,y)
// The pseg base address and the pseg size must be multiple of 2 Mbytes
// (one big page).
// - base is the pseg local base address (no cluster extension)
// - size is the pseg length (bytes)
// The first page in cluster[0][0] is reserved for identity mapped vsegs.
///////////////////////////////////////////////////////////////////////////////////
void _pmem_alloc_init( unsigned int x,
                       unsigned int y,
                       unsigned int base,
                       unsigned int size );

///////////////////////////////////////////////////////////////////////////////////
// This function allocates n contiguous small pages (4 Kbytes), 
// from the physical memory allocator defined by the p pointer. 
// It returns the PPN (28 bits) of the first physical small page. 
// Exit if not enough free space.
///////////////////////////////////////////////////////////////////////////////////
unsigned int _get_small_ppn( pmem_alloc_t* p,
                             unsigned int  n );

///////////////////////////////////////////////////////////////////////////////////
// This function allocates n contiguous big pages (2 Mbytes), 
// from the physical memory allocator defined by the p pointer. 
// It returns the PPN (28 bits) of the first physical big page
// (the SPPI field of the ppn is always 0). 
// Exit if not enough free space.
///////////////////////////////////////////////////////////////////////////////////
unsigned int _get_big_ppn( pmem_alloc_t* p,
                           unsigned int  n );

#endif 

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

