///////////////////////////////////////////////////////////////////////////////////
// File     : pmem.c
// Date     : 01/07/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <tty0.h>
#include <utils.h>
#include <pmem.h>
#include <giet_config.h>

///////////////////////////////////////////////////////////////////////////////////
//     Global variable : array of physical memory allocators (one per cluster)
///////////////////////////////////////////////////////////////////////////////////

extern pmem_alloc_t boot_pmem_alloc[X_SIZE][Y_SIZE];

////////////////////////////////////////
void _pmem_alloc_init( unsigned int x,
                       unsigned int y,
                       unsigned int base,
                       unsigned int size )
{
    if ( (base & 0x1FFFFF) || (size & 0x1FFFFF) )
    {
        _puts("\n[GIET ERROR] in _pmem_alloc_init() : pseg in cluster[");
        _putd( x );
        _puts(",");
        _putd( y );
        _puts("] not aligned on 2 Mbytes\n");
        _exit();
    }

    pmem_alloc_t* p       = &boot_pmem_alloc[x][y];

    unsigned int  bppi_min = base >> 21;
    unsigned int  bppi_max = (base + size) >> 21;

    p->x        = x;
    p->y        = y;

    p->nxt_bppi = bppi_min;
    p->max_bppi = bppi_max;

    p->nxt_sppi = 0;
    p->max_sppi = 0;

    // first page reserved in cluster [0][0]
    if ( (x==0) && (y==0) ) p->nxt_bppi = p->nxt_bppi + 1;

} // end pmem_alloc_init()

/////////////////////////////////////////////
unsigned int _get_big_ppn( pmem_alloc_t*  p, 
                           unsigned int   n )
{
    unsigned int x   = p->x;
    unsigned int y   = p->y;
    unsigned int bpi = p->nxt_bppi;  // bpi : BPPI of the first allocated big page

    if ( (bpi + n) > p->max_bppi )
    {
        _puts("\n[GIET ERROR] in _get_big_ppn() : not enough BPP in cluster[");
        _putd( x );
        _puts(",");
        _putd( y );
        _puts("]\n");
        _exit();
    }
    
    // update allocator state
    p->nxt_bppi = bpi + n;

    return (x << 24) + (y << 20) + (bpi << 9);

} // end get_big_ppn()

///////////////////////////////////////////////
unsigned int _get_small_ppn( pmem_alloc_t*  p,
                             unsigned int   n )
{
    unsigned int x    = p->x;
    unsigned int y    = p->y;
    unsigned int spi  = p->nxt_sppi;   // spi : SPPI of the first allocated small page 
    unsigned int bpi  = p->spp_bppi;   // bpi : BPPI of the first allocated small page

    // get a new big page if not enough contiguous small pages
    // in the big page currently used to allocate small pages
    if ( spi + n > p->max_sppi )  
    {
        if ( p->nxt_bppi + 1 > p->max_bppi )
        {
            _puts("\n[GIET ERROR] in _get_small_ppn() : not enough BPP in cluster[");
            _putd( x );
            _puts(",");
            _putd( y );
            _puts("]\n");
            _exit();
        }

        // update the allocator state for the new big page
        p->spp_bppi = p->nxt_bppi;
        p->nxt_bppi = p->nxt_bppi + 1;
        p->nxt_sppi = 0;
        p->max_sppi = 512;

        // set the spi and bpi values
        spi = p->nxt_sppi;
        bpi = p->spp_bppi;
    }

    // update allocator state for the n small pages
    p->nxt_sppi = p->nxt_sppi + n;

    return (x << 24) + (y << 20) + (bpi << 9) + spi;

} // end _get_small_ppn()



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4


