///////////////////////////////////////////////////////////////////////////////////
// File     : boot.c
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The boot.c file contains the bootloader for the GIET-VM static OS.  
//
// This code has been written for the MIPS32 processor.
// The virtual adresses are on 32 bits and use the (unsigned int) type. The 
// physicals addresses can have up to 40 bits, and use type (unsigned long long).
// It natively supports clusterised shared memory multi-processors architectures, 
// where each processor is identified by a composite index [x,y,p],
// and where there is one physical memory bank per cluster.
//
// The boot.elf file is stored on disk and is loaded into memory by proc[0,0,0],
// executing the generic preloader (stored in ROM). The boot-loader code itself
// is executed in parallel by all proc[x,y,0], and performs the following tasks:
// - load into memory various binary files, from a FAT32 file system.
// - build the various page tables (one page table per vspace). 
// - initialize the shedulers (one scheduler per processor).
//
// 1) The binary files to be loaded are:
//    - the "map.bin" file contains the hardware architecture description, 
//      the set of user applications that will be mapped on the architecture, 
//      and the mapping directives. The mapping includes the placement of tasks 
//      on processors, and the placement of virtual segments on the physical 
//      segments. It must be stored in the the seg_boot_mapping segment 
//      (at address SEG_BOOT_MAPPING_BASE defined in hard_config.h file).
//    - the "kernel.elf" file contains the kernel binary code and data.
//    - the various "application.elf" files.
//
// 2) The GIET-VM uses the paged virtual memory to provides two services:
//    - classical memory protection, when several independant applications compiled
//      in different virtual spaces are executing on the same hardware platform.
//    - data placement in NUMA architectures, to control the placement 
//      of the software objects (vsegs) on the physical memory banks (psegs).
//    The max number of vspaces (GIET_NB_VSPACE_MAX) is a configuration parameter.
//    The page table are statically build in the boot phase, and they do not 
//    change during execution. For each application, the page tables are replicated
//    in all clusters.
//    The GIET_VM uses both small pages (4 Kbytes), and big pages (2 Mbytes). 
//    Each page table (one page table per virtual space) is monolithic, and 
//    contains one PT1 (8 Kbytes) and a variable number of PT2s (4 Kbytes each). 
//    For each vspace, the max number of PT2s is defined by the size of the PTAB 
//    vseg in the mapping.
//    The PT1 is indexed by the ix1 field (11 bits) of the VPN. An entry is 32 bits.
//    A PT2 is indexed the ix2 field (9 bits) of the VPN. An entry is 64 bits.
//    The first word contains the flags, the second word contains the PPN.
//
// 3) The Giet-VM implement one private scheduler per processor.
//    For each application, the tasks are statically allocated to processors
//    and there is no task migration during execution.
//    Each sheduler occupies 8K bytes, and contains up to 14 task contexts 
//    The task context [13] is reserved for the "idle" task that does nothing, and
//    is launched by the scheduler when there is no other runable task.
///////////////////////////////////////////////////////////////////////////////////
// Implementation Notes:
//
// 1) The cluster_id variable is a linear index in the mapping_info array.
//    The cluster_xy variable is the tological index = x << Y_WIDTH + y
// 
// 2) We set the _tty0_boot_mode variable to force the _printf() function to use
//    the tty0_spin_lock for exclusive access to TTY0.
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <mapping_info.h>
#include <kernel_malloc.h>
#include <memspace.h>
#include <tty_driver.h>
#include <xcu_driver.h>
#include <bdv_driver.h>
#include <hba_driver.h>
#include <sdc_driver.h>
#include <cma_driver.h>
#include <nic_driver.h>
#include <iob_driver.h>
#include <pic_driver.h>
#include <mwr_driver.h>
#include <dma_driver.h>
#include <mmc_driver.h>
#include <ctx_handler.h>
#include <irq_handler.h>
#include <vmem.h>
#include <pmem.h>
#include <utils.h>
#include <tty0.h>
#include <kernel_locks.h>
#include <kernel_barriers.h>
#include <elf-types.h>
#include <fat32.h>
#include <mips32_registers.h>
#include <stdarg.h>

#if !defined(X_SIZE)
# error: The X_SIZE value must be defined in the 'hard_config.h' file !
#endif

#if !defined(Y_SIZE)
# error: The Y_SIZE value must be defined in the 'hard_config.h' file !
#endif

#if !defined(X_WIDTH)
# error: The X_WIDTH value must be defined in the 'hard_config.h' file !
#endif

#if !defined(Y_WIDTH)
# error: The Y_WIDTH value must be defined in the 'hard_config.h' file !
#endif

#if !defined(SEG_BOOT_MAPPING_BASE) 
# error: The SEG_BOOT_MAPPING_BASE value must be defined in the hard_config.h file !
#endif

#if !defined(NB_PROCS_MAX)
# error: The NB_PROCS_MAX value must be defined in the 'hard_config.h' file !
#endif

#if !defined(GIET_NB_VSPACE_MAX)
# error: The GIET_NB_VSPACE_MAX value must be defined in the 'giet_config.h' file !
#endif

#if !defined(GIET_ELF_BUFFER_SIZE) 
# error: The GIET_ELF_BUFFER_SIZE value must be defined in the giet_config.h file !
#endif

////////////////////////////////////////////////////////////////////////////
//      Global variables for boot code
////////////////////////////////////////////////////////////////////////////

// Temporaty buffer used to load one complete .elf file  
__attribute__((section(".kdata")))
unsigned char  _boot_elf_buffer[GIET_ELF_BUFFER_SIZE] __attribute__((aligned(64)));

// Physical memory allocators array (one per cluster)
__attribute__((section(".kdata")))
pmem_alloc_t  boot_pmem_alloc[X_SIZE][Y_SIZE];

// Schedulers virtual base addresses array (one per processor)
__attribute__((section(".kdata")))
static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX];

// Page tables virtual base addresses (one per vspace and per cluster)
__attribute__((section(".kdata")))
unsigned int        _ptabs_vaddr[GIET_NB_VSPACE_MAX][X_SIZE][Y_SIZE];

// Page tables physical base addresses (one per vspace and per cluster)
__attribute__((section(".kdata")))
paddr_t             _ptabs_paddr[GIET_NB_VSPACE_MAX][X_SIZE][Y_SIZE];

// Page tables pt2 allocators (one per vspace and per cluster)
__attribute__((section(".kdata")))
unsigned int        _ptabs_next_pt2[GIET_NB_VSPACE_MAX][X_SIZE][Y_SIZE];

// Page tables max_pt2  (same value for all page tables)
__attribute__((section(".kdata")))
unsigned int        _ptabs_max_pt2;

// boot code uses a spin lock to protect TTY0
__attribute__((section(".kdata")))
unsigned int        _tty0_boot_mode = 1;

// boot code does not uses a lock to protect HBA command allocator
__attribute__((section(".kdata")))
unsigned int        _hba_boot_mode = 1;

__attribute__((section(".kdata")))
spin_lock_t         _ptabs_spin_lock[GIET_NB_VSPACE_MAX][X_SIZE][Y_SIZE];

// barrier used by boot code for parallel execution
__attribute__((section(".kdata")))
simple_barrier_t    _barrier_all_clusters;

//////////////////////////////////////////////////////////////////////////////
//        Extern variables
//////////////////////////////////////////////////////////////////////////////

// this variable is allocated in the tty0.c file
extern spin_lock_t  _tty0_spin_lock;

// this variable is allocated in the mmc_driver.c
extern unsigned int _mmc_boot_mode;

extern void boot_entry();

//////////////////////////////////////////////////////////////////////////////
// This function registers a new PTE1 in the page table defined
// by the vspace_id argument, and the (x,y) coordinates.
// It updates only the first level PT1.
// As each vseg is mapped by a different processor, the PT1 entry cannot
// be concurrently accessed, and we don't need to take any lock.
//////////////////////////////////////////////////////////////////////////////
void boot_add_pte1( unsigned int vspace_id,
                    unsigned int x,
                    unsigned int y,
                    unsigned int vpn,        // 20 bits right-justified
                    unsigned int flags,      // 10 bits left-justified 
                    unsigned int ppn )       // 28 bits right-justified
{
    // compute index in PT1
    unsigned int    ix1 = vpn >> 9;         // 11 bits for ix1

    // get page table physical base address 
    paddr_t  pt1_pbase = _ptabs_paddr[vspace_id][x][y];

    if ( pt1_pbase == 0 )
    {
        _printf("\n[BOOT ERROR] in boot_add_pte1() : no PTAB in cluster[%d,%d]"
                    " containing processors\n", x , y );
        _exit();
    }

    // compute pte1 : 2 bits V T / 8 bits flags / 3 bits RSVD / 19 bits bppi
    unsigned int    pte1 = PTE_V |
                           (flags & 0x3FC00000) |
                           ((ppn>>9) & 0x0007FFFF);

    // write pte1 in PT1
    _physical_write( pt1_pbase + 4*ix1, pte1 );

    asm volatile ("sync");

}   // end boot_add_pte1()

//////////////////////////////////////////////////////////////////////////////
// This function registers a new PTE2 in the page table defined
// by the vspace_id argument, and the (x,y) coordinates.
// It updates both the first level PT1 and the second level PT2.
// As the set of PT2s is implemented as a fixed size array (no dynamic 
// allocation), this function checks a possible overflow of the PT2 array.
// As a given entry in PT1 can be shared by several vsegs, mapped by 
// different processors, we need to take the lock protecting PTAB[v][x]y].
//////////////////////////////////////////////////////////////////////////////
void boot_add_pte2( unsigned int vspace_id,
                    unsigned int x,
                    unsigned int y,
                    unsigned int vpn,        // 20 bits right-justified
                    unsigned int flags,      // 10 bits left-justified 
                    unsigned int ppn )       // 28 bits right-justified
{
    unsigned int ix1;
    unsigned int ix2;
    paddr_t      pt2_pbase;     // PT2 physical base address
    paddr_t      pte2_paddr;    // PTE2 physical address
    unsigned int pt2_id;        // PT2 index
    unsigned int ptd;           // PTD : entry in PT1

    ix1 = vpn >> 9;             // 11 bits for ix1
    ix2 = vpn & 0x1FF;          //  9 bits for ix2

    // get page table physical base address 
    paddr_t      pt1_pbase = _ptabs_paddr[vspace_id][x][y];

    if ( pt1_pbase == 0 )
    {
        _printf("\n[BOOT ERROR] in boot_add_pte2() : no PTAB for vspace %d "
                "in cluster[%d,%d]\n", vspace_id , x , y );
        _exit();
    }

    // get lock protecting PTAB[vspace_id][x][y]
    _spin_lock_acquire( &_ptabs_spin_lock[vspace_id][x][y] );

    // get ptd in PT1
    ptd = _physical_read( pt1_pbase + 4 * ix1 );

    if ((ptd & PTE_V) == 0)    // undefined PTD: compute PT2 base address, 
                               // and set a new PTD in PT1 
    {
        // get a new pt2_id
        pt2_id = _ptabs_next_pt2[vspace_id][x][y];
        _ptabs_next_pt2[vspace_id][x][y] = pt2_id + 1;

        // check overflow
        if (pt2_id == _ptabs_max_pt2) 
        {
            _printf("\n[BOOT ERROR] in boot_add_pte2() : PTAB[%d,%d,%d]"
                    " contains not enough PT2s\n", vspace_id, x, y );
            _exit();
        }

        pt2_pbase = pt1_pbase + PT1_SIZE + PT2_SIZE * pt2_id;
        ptd = PTE_V | PTE_T | (unsigned int) (pt2_pbase >> 12);

        // set PTD into PT1
        _physical_write( pt1_pbase + 4*ix1, ptd);
    }
    else                       // valid PTD: compute PT2 base address
    {
        pt2_pbase = ((paddr_t)(ptd & 0x0FFFFFFF)) << 12;
    }

    // set PTE in PT2 : flags & PPN in two 32 bits words
    pte2_paddr  = pt2_pbase + 8 * ix2;
    _physical_write(pte2_paddr     , (PTE_V | flags) );
    _physical_write(pte2_paddr + 4 , ppn );

    // release lock protecting PTAB[vspace_id][x][y]
    _spin_lock_release( &_ptabs_spin_lock[vspace_id][x][y] );

    asm volatile ("sync");

}   // end boot_add_pte2()

////////////////////////////////////////////////////////////////////////////////////
// Align the value of paddr or vaddr to the required alignement,
// defined by alignPow2 == L2(alignement).
////////////////////////////////////////////////////////////////////////////////////
paddr_t paddr_align_to( paddr_t paddr, unsigned int alignPow2 ) 
{
    paddr_t mask = (1 << alignPow2) - 1;
    return ((paddr + mask) & ~mask);
}

unsigned int vaddr_align_to( unsigned int vaddr, unsigned int alignPow2 ) 
{
    unsigned int mask = (1 << alignPow2) - 1;
    return ((vaddr + mask) & ~mask);
}

/////////////////////////////////////////////////////////////////////////////////////
// This function map a vseg identified by the vseg pointer.
//
// A given vseg can be mapped in a Big Physical Pages (BPP: 2 Mbytes) or in a
// Small Physical Pages (SPP: 4 Kbytes), depending on the "big" attribute of vseg,
// with the following rules:
// - SPP : There is only one vseg in a small physical page, but a single vseg
//   can cover several contiguous small physical pages.
// - BPP : It can exist several vsegs in a single big physical page, and a single
//   vseg can cover several contiguous big physical pages.
//
// 1) First step: it computes various vseg attributes and checks 
//    alignment constraints.
//
// 2) Second step: it allocates the required number of contiguous physical pages, 
//    computes the physical base address (if the vseg is not identity mapping),
//    and register it in the vseg pbase field.
//    Only the vsegs used by the boot code and the peripheral vsegs
//    can be identity mapping. The first big physical page in cluster[0,0] 
//    is reserved for the boot vsegs.
//
// 3) Third step (only for vseg that have the VSEG_TYPE_PTAB): the M page tables
//    associated to the M vspaces must be packed in the same vseg.
//    We divide this vseg in M sub-segments, and compute the vbase and pbase
//    addresses for M page tables, and register these addresses in the _ptabs_paddr
//    and _ptabs_vaddr arrays.
//  
/////////////////////////////////////////////////////////////////////////////////////
void boot_vseg_map( mapping_vseg_t* vseg,
                    unsigned int    vspace_id )
{
    mapping_header_t*   header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t*  cluster = _get_cluster_base(header);
    mapping_pseg_t*     pseg    = _get_pseg_base(header);

    //////////// First step : compute vseg attributes 

    // compute destination cluster pointer & coordinates
    pseg    = pseg + vseg->psegid;
    cluster = cluster + pseg->clusterid;
    unsigned int        x_dest     = cluster->x;
    unsigned int        y_dest     = cluster->y;

    // compute the "big" vseg attribute
    unsigned int        big = vseg->big;

    // all vsegs must be aligned on 4Kbytes
    if ( vseg->vbase & 0x00000FFF ) 
    {
        _printf("\n[BOOT ERROR] vseg %s not aligned : vbase = %x\n", 
                vseg->name, vseg->vbase );
        _exit();
    }

    // compute the "is_ram" vseg attribute
    unsigned int        is_ram;
    if ( pseg->type == PSEG_TYPE_RAM )  is_ram = 1;
    else                                is_ram = 0;

    // compute the "is_ptab" attribute
    unsigned int        is_ptab;
    if ( vseg->type == VSEG_TYPE_PTAB ) is_ptab = 1;
    else                                is_ptab = 0;

    // compute actual vspace index
    unsigned int vsid;
    if ( vspace_id == 0xFFFFFFFF ) vsid = 0;
    else                           vsid = vspace_id;

    //////////// Second step : compute ppn and npages  
    //////////// - if identity mapping :  ppn <= vpn 
    //////////// - if vseg is periph   :  ppn <= pseg.base >> 12
    //////////// - if vseg is ram      :  ppn <= physical memory allocator

    unsigned int ppn;          // first physical page index (28 bits = |x|y|bppi|sppi|) 
    unsigned int vpn;          // first virtual page index  (20 bits = |ix1|ix2|)
    unsigned int vpn_max;      // last  virtual page index  (20 bits = |ix1|ix2|)

    vpn     = vseg->vbase >> 12;
    vpn_max = (vseg->vbase + vseg->length - 1) >> 12;

    // compute npages
    unsigned int npages;       // number of required (big or small) pages
    if ( big == 0 ) npages  = vpn_max - vpn + 1;            // number of small pages
    else            npages  = (vpn_max>>9) - (vpn>>9) + 1;  // number of big pages

    // compute ppn
    if ( vseg->ident )           // identity mapping
    {
        ppn = vpn;
    }
    else                         // not identity mapping
    {
        if ( is_ram )            // RAM : physical memory allocation required
        {
            // compute pointer on physical memory allocator in dest cluster
            pmem_alloc_t*     palloc = &boot_pmem_alloc[x_dest][y_dest];

            if ( big == 0 )             // SPP : small physical pages
            {
                // allocate contiguous small physical pages
                ppn = _get_small_ppn( palloc, npages );
            }
            else                            // BPP : big physical pages
            {
 
                // one big page can be shared by several vsegs 
                // we must chek if BPP already allocated 
                if ( is_ptab )   // It cannot be mapped
                {
                    ppn = _get_big_ppn( palloc, npages ); 
                }
                else             // It can be mapped
                {
                    unsigned int ix1   = vpn >> 9;   // 11 bits
                    paddr_t      paddr = _ptabs_paddr[vsid][x_dest][y_dest] + (ix1<<2);
                    unsigned int pte1  = _physical_read( paddr );

                    if ( (pte1 & PTE_V) == 0 )     // BPP not allocated yet
                    {
                        // allocate contiguous big physical pages 
                        ppn = _get_big_ppn( palloc, npages );
                    }
                    else                           // BPP already allocated
                    {
                        // test if new vseg has the same mode bits than
                        // the other vsegs in the same big page
                        unsigned int pte1_mode = 0;
                        if (pte1 & PTE_C) pte1_mode |= C_MODE_MASK;
                        if (pte1 & PTE_X) pte1_mode |= X_MODE_MASK;
                        if (pte1 & PTE_W) pte1_mode |= W_MODE_MASK;
                        if (pte1 & PTE_U) pte1_mode |= U_MODE_MASK;
                        if (vseg->mode != pte1_mode) 
                        {
                            _printf("\n[BOOT ERROR] in boot_vseg_map() : "
                                    "vseg %s has different flags than another vseg "
                                    "in the same BPP\n", vseg->name );
                            _exit();
                        }
                        ppn = ((pte1 << 9) & 0x0FFFFE00);
                    }
                }
                ppn = ppn | (vpn & 0x1FF);
            }
        }
        else                    // PERI : no memory allocation required
        {
            ppn = pseg->base >> 12;
        }
    }

    // update vseg.pbase field and update vsegs chaining
    vseg->pbase     = ((paddr_t)ppn) << 12;
    vseg->mapped    = 1;


    //////////// Third step : (only if the vseg is a page table)
    //////////// - compute the physical & virtual base address for each vspace
    ////////////   by dividing the vseg in several sub-segments.
    //////////// - register it in _ptabs_vaddr & _ptabs_paddr arrays,
    ////////////   and initialize next_pt2 allocators.
    //////////// - reset all entries in first level page tables
    
    if ( is_ptab )
    {
        unsigned int   vs;        // vspace index
        unsigned int   nspaces;   // number of vspaces
        unsigned int   nsp;       // number of small pages for one PTAB
        unsigned int   offset;    // address offset for current PTAB

        nspaces = header->vspaces;
        offset  = 0;

        // each PTAB must be aligned on a 8 Kbytes boundary
        nsp = ( vseg->length >> 12 ) / nspaces;
        if ( (nsp & 0x1) == 0x1 ) nsp = nsp - 1;

        // compute max_pt2
        _ptabs_max_pt2 = ((nsp<<12) - PT1_SIZE) / PT2_SIZE;

        for ( vs = 0 ; vs < nspaces ; vs++ )
        {
            _ptabs_vaddr   [vs][x_dest][y_dest] = (vpn + offset) << 12;
            _ptabs_paddr   [vs][x_dest][y_dest] = ((paddr_t)(ppn + offset)) << 12;
            _ptabs_next_pt2[vs][x_dest][y_dest] = 0;
            offset += nsp;

            // reset all entries in PT1 (8 Kbytes)
            _physical_memset( _ptabs_paddr[vs][x_dest][y_dest], PT1_SIZE, 0 );
        }
    }

    asm volatile ("sync");

#if BOOT_DEBUG_PT
if ( big )
_printf("\n[BOOT] vseg %s : cluster[%d,%d] / "
       "vbase = %x / length = %x / BIG    / npages = %d / pbase = %l\n",
       vseg->name, x_dest, y_dest, vseg->vbase, vseg->length, npages, vseg-> pbase );
else
_printf("\n[BOOT] vseg %s : cluster[%d,%d] / "
        "vbase = %x / length = %x / SMALL / npages = %d / pbase = %l\n",
       vseg->name, x_dest, y_dest, vseg->vbase, vseg->length, npages, vseg-> pbase );
#endif

} // end boot_vseg_map()

/////////////////////////////////////////////////////////////////////////////////////
// For the vseg defined by the vseg pointer, this function register PTEs
// in one or several page tables.
// It is a global vseg (kernel vseg) if (vspace_id == 0xFFFFFFFF).
// The number of involved PTABs depends on the "local" and "global" attributes:
//  - PTEs are replicated in all vspaces for a global vseg.
//  - PTEs are replicated in all clusters containing procs for a non local vseg.
/////////////////////////////////////////////////////////////////////////////////////
void boot_vseg_pte( mapping_vseg_t*  vseg,
                    unsigned int     vspace_id )
{
    // compute the "global" vseg attribute and actual vspace index
    unsigned int        global;
    unsigned int        vsid;    
    if ( vspace_id == 0xFFFFFFFF )
    {
        global = 1;
        vsid   = 0;
    }
    else
    {
        global = 0;
        vsid   = vspace_id;
    }

    // compute the "local" and "big" attributes 
    unsigned int        local  = vseg->local;
    unsigned int        big    = vseg->big;

    // compute vseg flags
    // The three flags (Local, Remote and Dirty) are set to 1 
    // to avoid hardware update for these flags, because GIET_VM 
    // does use these flags.
    unsigned int flags = 0;
    if (vseg->mode & C_MODE_MASK) flags |= PTE_C;
    if (vseg->mode & X_MODE_MASK) flags |= PTE_X;
    if (vseg->mode & W_MODE_MASK) flags |= PTE_W;
    if (vseg->mode & U_MODE_MASK) flags |= PTE_U;
    if ( global )                 flags |= PTE_G;
                                  flags |= PTE_L;
                                  flags |= PTE_R;
                                  flags |= PTE_D;

    // compute VPN, PPN and number of pages (big or small) 
    unsigned int vpn     = vseg->vbase >> 12;
    unsigned int vpn_max = (vseg->vbase + vseg->length - 1) >> 12;
    unsigned int ppn     = (unsigned int)(vseg->pbase >> 12);
    unsigned int npages;
    if ( big == 0 ) npages  = vpn_max - vpn + 1;            
    else            npages  = (vpn_max>>9) - (vpn>>9) + 1; 

    // compute destination cluster coordinates, for local vsegs
    mapping_header_t*   header       = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t*  cluster      = _get_cluster_base(header);
    mapping_pseg_t*     pseg         = _get_pseg_base(header);
    mapping_pseg_t*     pseg_dest    = &pseg[vseg->psegid];
    mapping_cluster_t*  cluster_dest = &cluster[pseg_dest->clusterid];
    unsigned int        x_dest       = cluster_dest->x;
    unsigned int        y_dest       = cluster_dest->y;

    unsigned int p;           // iterator for physical page index
    unsigned int x;           // iterator for cluster x coordinate  
    unsigned int y;           // iterator for cluster y coordinate  
    unsigned int v;           // iterator for vspace index 

    // loop on PTEs
    for ( p = 0 ; p < npages ; p++ )
    { 
        if  ( (local != 0) && (global == 0) )         // one cluster  / one vspace
        {
            if ( big )   // big pages => PTE1s
            {
                boot_add_pte1( vsid,
                               x_dest,
                               y_dest,
                               vpn + (p<<9),
                               flags, 
                               ppn + (p<<9) );
            }
            else         // small pages => PTE2s
            {
                boot_add_pte2( vsid,
                               x_dest,
                               y_dest,
                               vpn + p,      
                               flags, 
                               ppn + p );
            }
        }
        else if ( (local == 0) && (global == 0) )     // all clusters / one vspace
        {
            for ( x = 0 ; x < X_SIZE ; x++ )
            {
                for ( y = 0 ; y < Y_SIZE ; y++ )
                {
                    if ( cluster[(x * Y_SIZE) + y].procs )
                    {
                        if ( big )   // big pages => PTE1s
                        {
                            boot_add_pte1( vsid,
                                           x,
                                           y,
                                           vpn + (p<<9),
                                           flags, 
                                           ppn + (p<<9) );
                        }
                        else         // small pages => PTE2s
                        {
                            boot_add_pte2( vsid,
                                           x,
                                           y,
                                           vpn + p,
                                           flags, 
                                           ppn + p );
                        }
                    }
                }
            }
        }
        else if ( (local != 0) && (global != 0) )     // one cluster  / all vspaces 
        {
            for ( v = 0 ; v < header->vspaces ; v++ )
            {
                if ( big )   // big pages => PTE1s
                {
                    boot_add_pte1( v,
                                   x_dest,
                                   y_dest,
                                   vpn + (p<<9),
                                   flags, 
                                   ppn + (p<<9) );
                }
                else         // small pages = PTE2s
                { 
                    boot_add_pte2( v,
                                   x_dest,
                                   y_dest,
                                   vpn + p,
                                   flags, 
                                   ppn + p );
                }
            }
        }
        else if ( (local == 0) && (global != 0) )     // all clusters / all vspaces
        {
            for ( x = 0 ; x < X_SIZE ; x++ )
            {
                for ( y = 0 ; y < Y_SIZE ; y++ )
                {
                    if ( cluster[(x * Y_SIZE) + y].procs )
                    {
                        for ( v = 0 ; v < header->vspaces ; v++ )
                        {
                            if ( big )  // big pages => PTE1s
                            {
                                boot_add_pte1( v,
                                               x,
                                               y,
                                               vpn + (p<<9),
                                               flags, 
                                               ppn + (p<<9) );
                            }
                            else        // small pages -> PTE2s
                            {
                                boot_add_pte2( v,
                                               x,
                                               y,
                                               vpn + p,
                                               flags, 
                                               ppn + p );
                            }
                        }
                    }
                }
            }
        }
    }  // end for pages

    asm volatile ("sync");

}  // end boot_vseg_pte()


///////////////////////////////////////////////////////////////////////////////
// This function is executed by  processor[x][y][0] in each cluster 
// containing at least one processor.
// It initialises all page table for all global or private vsegs
// mapped in cluster[x][y], as specified in the mapping.
// In each cluster all page tables for the different vspaces must be
// packed in one vseg occupying one single BPP (Big Physical Page).
// 
// For each vseg, the mapping is done in two steps:
// 1) mapping : the boot_vseg_map() function allocates contiguous BPPs 
//    or SPPs (if the vseg is not associated to a peripheral), and register
//    the physical base address in the vseg pbase field. It initialises the
//    _ptabs_vaddr[] and _ptabs_paddr[] arrays if the vseg is a PTAB.
//
// 2) page table initialisation : the boot_vseg_pte() function initialise 
//    the PTEs (both PTE1 and PTE2) in one or several page tables:
//    - PTEs are replicated in all vspaces for a global vseg.
//    - PTEs are replicated in all clusters for a non local vseg.
//
// We must handle vsegs in the following order
//   1) global vseg containing PTAB mapped in cluster[x][y],
//   2) global vsegs occupying more than one BPP mapped in cluster[x][y], 
//   3) others global vsegs mapped in cluster[x][y],
//   4) all private vsegs in all user spaces mapped in cluster[x][y].
///////////////////////////////////////////////////////////////////////////////
void boot_ptab_init( unsigned int cx,
                     unsigned int cy ) 
{
    mapping_header_t*   header = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t*   vspace = _get_vspace_base(header);
    mapping_vseg_t*     vseg   = _get_vseg_base(header);
    mapping_cluster_t*  cluster ;
    mapping_pseg_t*     pseg    ;

    unsigned int vspace_id;
    unsigned int vseg_id;

    unsigned int procid     = _get_procid();
    unsigned int lpid       = procid & ((1<<P_WIDTH)-1);

    if( lpid )
    {
        _printf("\n[BOOT ERROR] in boot_ptab_init() : "
                "P[%d][%d][%d] should not execute it\n", cx, cy, lpid );
        _exit();
    } 

    if ( header->vspaces == 0 )
    {
        _printf("\n[BOOT ERROR] in boot_ptab_init() : "
                "mapping %s contains no vspace\n", header->name );
        _exit();
    }

    ///////// Phase 1 : global vseg containing the PTAB (two barriers required)

    // get PTAB global vseg in cluster(cx,cy)
    unsigned int found = 0;
    for (vseg_id = 0; vseg_id < header->globals; vseg_id++) 
    {
        pseg    = _get_pseg_base(header) + vseg[vseg_id].psegid;
        cluster = _get_cluster_base(header) + pseg->clusterid;
        if ( (vseg[vseg_id].type == VSEG_TYPE_PTAB) && 
             (cluster->x == cx) && (cluster->y == cy) )
        {
            found = 1;
            break;
        }
    }
    if ( found == 0 )
    {
        _printf("\n[BOOT ERROR] in boot_ptab_init() : "
                "cluster[%d][%d] contains no PTAB vseg\n", cx , cy );
        _exit();
    }

    boot_vseg_map( &vseg[vseg_id], 0xFFFFFFFF );

    //////////////////////////////////////////////
    _simple_barrier_wait( &_barrier_all_clusters );
    //////////////////////////////////////////////

    boot_vseg_pte( &vseg[vseg_id], 0xFFFFFFFF );

    //////////////////////////////////////////////
    _simple_barrier_wait( &_barrier_all_clusters );
    //////////////////////////////////////////////

    ///////// Phase 2 : global vsegs occupying more than one BPP 

    for (vseg_id = 0; vseg_id < header->globals; vseg_id++) 
    {
        pseg    = _get_pseg_base(header) + vseg[vseg_id].psegid;
        cluster = _get_cluster_base(header) + pseg->clusterid;
        if ( (vseg[vseg_id].length > 0x200000) &&
             (vseg[vseg_id].mapped == 0) &&
             (cluster->x == cx) && (cluster->y == cy) )
        {
            boot_vseg_map( &vseg[vseg_id], 0xFFFFFFFF );
            boot_vseg_pte( &vseg[vseg_id], 0xFFFFFFFF );
        }
    }

    ///////// Phase 3 : all others global vsegs 

    for (vseg_id = 0; vseg_id < header->globals; vseg_id++) 
    { 
        pseg    = _get_pseg_base(header) + vseg[vseg_id].psegid;
        cluster = _get_cluster_base(header) + pseg->clusterid;
        if ( (vseg[vseg_id].mapped == 0) && 
             (cluster->x == cx) && (cluster->y == cy) )
        {
            boot_vseg_map( &vseg[vseg_id], 0xFFFFFFFF );
            boot_vseg_pte( &vseg[vseg_id], 0xFFFFFFFF );
        }
    }

    ///////// Phase 4 : all private vsegs 

    for (vspace_id = 0; vspace_id < header->vspaces; vspace_id++) 
    {
        for (vseg_id = vspace[vspace_id].vseg_offset;
             vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs);
             vseg_id++) 
        {
            pseg    = _get_pseg_base(header) + vseg[vseg_id].psegid;
            cluster = _get_cluster_base(header) + pseg->clusterid;
            if ( (cluster->x == cx) && (cluster->y == cy) )
            {
                boot_vseg_map( &vseg[vseg_id], vspace_id );
                boot_vseg_pte( &vseg[vseg_id], vspace_id );
            }
        }
    }

    //////////////////////////////////////////////
    _simple_barrier_wait( &_barrier_all_clusters );
    //////////////////////////////////////////////

} // end boot_ptab_init()

////////////////////////////////////////////////////////////////////////////////
// This function should be executed by P[0][0][0] only. It complete the
// page table initialisation, taking care of all global vsegs that are
// not mapped in a cluster containing a processor, and have not been
// handled by the boot_ptab_init(x,y) function.
// An example of such vsegs are the external peripherals in TSAR_LETI platform.
////////////////////////////////////////////////////////////////////////////////
void boot_ptab_extend()
{

    mapping_header_t*   header = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vseg_t*     vseg   = _get_vseg_base(header);

    unsigned int vseg_id;

    for (vseg_id = 0; vseg_id < header->globals; vseg_id++) 
    {
        if ( vseg[vseg_id].mapped == 0 )  
        {
            boot_vseg_map( &vseg[vseg_id], 0xFFFFFFFF );
            boot_vseg_pte( &vseg[vseg_id], 0xFFFFFFFF );
        }
    }
}  // end boot_ptab_extend()

///////////////////////////////////////////////////////////////////////////////
// This function returns in the vbase and length buffers the virtual base 
// address and the length of the  segment allocated to the schedulers array 
// in the cluster defined by the clusterid argument.
///////////////////////////////////////////////////////////////////////////////
void boot_get_sched_vaddr( unsigned int  cluster_id,
                           unsigned int* vbase, 
                           unsigned int* length )
{
    mapping_header_t* header = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vseg_t*   vseg   = _get_vseg_base(header);
    mapping_pseg_t*   pseg   = _get_pseg_base(header);

    unsigned int vseg_id;
    unsigned int found = 0;

    for ( vseg_id = 0 ; (vseg_id < header->vsegs) && (found == 0) ; vseg_id++ )
    {
        if ( (vseg[vseg_id].type == VSEG_TYPE_SCHED) && 
             (pseg[vseg[vseg_id].psegid].clusterid == cluster_id ) )
        {
            *vbase  = vseg[vseg_id].vbase;
            *length = vseg[vseg_id].length;
            found = 1;
        }
    }
    if ( found == 0 )
    {
        mapping_cluster_t* cluster = _get_cluster_base(header);
        _printf("\n[BOOT ERROR] No vseg of type SCHED in cluster [%d,%d]\n",
                cluster[cluster_id].x, cluster[cluster_id].y );
        _exit();
    }
} // end boot_get_sched_vaddr()

#if BOOT_DEBUG_SCHED
/////////////////////////////////////////////////////////////////////////////
// This debug function should be executed by only one procesor.
// It loops on all processors in all clusters to display
// the HWI / PTI / WTI interrupt vectors for each processor.
/////////////////////////////////////////////////////////////////////////////
void boot_sched_irq_display()
{
    unsigned int         cx;
    unsigned int         cy;
    unsigned int         lpid;
    unsigned int         slot;
    unsigned int         entry;
    unsigned int         type;
    unsigned int         channel;

    mapping_header_t*    header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t*   cluster = _get_cluster_base(header);

    static_scheduler_t*  psched; 

    for ( cx = 0 ; cx < X_SIZE ; cx++ )
    {
        for ( cy = 0 ; cy < Y_SIZE ; cy++ )
        {
            unsigned int cluster_id = (cx * Y_SIZE) + cy;
            unsigned int nprocs = cluster[cluster_id].procs;

            for ( lpid = 0 ; lpid < nprocs ; lpid++ )
            {
                psched = _schedulers[cx][cy][lpid];
        
                _printf("\n[BOOT] interrupt vectors for proc[%d,%d,%d]\n",
                        cx , cy , lpid );

                for ( slot = 0 ; slot < 32 ; slot++ )
                {
                    entry   = psched->hwi_vector[slot];
                    type    = entry & 0xFFFF;
                    channel = entry >> 16;
                    if ( type != ISR_DEFAULT )      
                    _printf(" - HWI : index = %d / type = %s / channel = %d\n",
                            slot , _isr_type_str[type] , channel );
                }
                for ( slot = 0 ; slot < 32 ; slot++ )
                {
                    entry   = psched->wti_vector[slot];
                    type    = entry & 0xFFFF;
                    channel = entry >> 16;
                    if ( type != ISR_DEFAULT )      
                    _printf(" - WTI : index = %d / type = %s / channel = %d\n",
                            slot , _isr_type_str[type] , channel );
                }
                for ( slot = 0 ; slot < 32 ; slot++ )
                {
                    entry   = psched->pti_vector[slot];
                    type    = entry & 0xFFFF;
                    channel = entry >> 16;
                    if ( type != ISR_DEFAULT )      
                    _printf(" - PTI : index = %d / type = %s / channel = %d\n",
                            slot , _isr_type_str[type] , channel );
                }
            }
        }
    } 
}  // end boot_sched_irq_display()
#endif


////////////////////////////////////////////////////////////////////////////////////
// This function is executed in parallel by all processors P[x][y][0].
// It initialises all schedulers in cluster [x][y]. The MMU must be activated.
// It is split in two phases separated by a synchronisation barrier.
// - In Step 1, it initialises the _schedulers[x][y][l] pointers array, the
//              idle_task context, the  HWI / PTI / WTI interrupt vectors,
//              and the CU HWI / PTI / WTI masks.
// - In Step 2, it scan all tasks in all vspaces to complete the tasks contexts, 
//              initialisation as specified in the mapping_info data structure,
//              and set the CP0_SCHED register. 
////////////////////////////////////////////////////////////////////////////////////
void boot_scheduler_init( unsigned int x, 
                          unsigned int y )
{
    mapping_header_t*    header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t*   cluster = _get_cluster_base(header);
    mapping_vspace_t*    vspace  = _get_vspace_base(header);
    mapping_vseg_t*      vseg    = _get_vseg_base(header);
    mapping_task_t*      task    = _get_task_base(header);
    mapping_periph_t*    periph  = _get_periph_base(header);
    mapping_irq_t*       irq     = _get_irq_base(header);

    unsigned int         periph_id; 
    unsigned int         irq_id;
    unsigned int         vspace_id;
    unsigned int         vseg_id;
    unsigned int         task_id; 

    unsigned int         sched_vbase;          // schedulers array vbase address 
    unsigned int         sched_length;         // schedulers array length
    static_scheduler_t*  psched;               // pointer on processor scheduler

    unsigned int cluster_id = (x * Y_SIZE) + y;
    unsigned int cluster_xy = (x << Y_WIDTH) + y;  
    unsigned int nprocs = cluster[cluster_id].procs;
    unsigned int lpid;                       
    
    if ( nprocs > 8 )
    {
        _printf("\n[BOOT ERROR] cluster[%d,%d] contains more than 8 procs\n", x, y );
        _exit();
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Step 1 : - initialize the schedulers[] array of pointers, 
    //          - initialize the "tasks" and "current variables. 
    //          - initialise the idle task context.
    //          - initialize the HWI, PTI and WTI interrupt vectors.
    //          - initialize the XCU masks for HWI / WTI / PTI interrupts.
    //
    // The general policy for interrupts routing is the following:         
    //          - the local HWI are statically allocatedted to local processors.
    //          - the nprocs first PTI are allocated for TICK (one per processor).
    //          - we allocate 4 WTI per processor: the first one is for WAKUP,
    //            the 3 others WTI are used for external interrupts (from PIC),
    //            and are dynamically allocated by kernel on demand.
    ///////////////////////////////////////////////////////////////////////////////

    // get scheduler array virtual base address in cluster[x,y]
    boot_get_sched_vaddr( cluster_id, &sched_vbase, &sched_length );

    if ( sched_length < (nprocs<<13) ) // 8 Kbytes per scheduler
    {
        _printf("\n[BOOT ERROR] Sched segment too small in cluster[%d,%d]\n",
                x, y );
        _exit();
    }

    // loop on local processors
    for ( lpid = 0 ; lpid < nprocs ; lpid++ )
    {
        // get scheduler pointer and initialise the schedulers pointers array
        psched = (static_scheduler_t*)(sched_vbase + (lpid<<13));
        _schedulers[x][y][lpid] = psched;

        // initialise the "tasks" and "current" variables default values
        psched->tasks   = 0;
        psched->current = IDLE_TASK_INDEX;

        // set default values for HWI / PTI / SWI vectors (valid bit = 0)
        unsigned int slot;
        for (slot = 0; slot < 32; slot++)
        {
            psched->hwi_vector[slot] = 0;
            psched->pti_vector[slot] = 0;
            psched->wti_vector[slot] = 0;
        }

        // initializes the idle_task context:
        // - the SR slot is 0xFF03 because this task run in kernel mode.
        // - it uses the page table of vspace[0]
        // - it uses the kernel TTY terminal
        // - slots containing addresses (SP,RA,EPC) are initialised by kernel_init()

        psched->context[IDLE_TASK_INDEX][CTX_CR_ID]    = 0;
        psched->context[IDLE_TASK_INDEX][CTX_SR_ID]    = 0xFF03;
        psched->context[IDLE_TASK_INDEX][CTX_PTPR_ID]  = _ptabs_paddr[0][x][y]>>13;
        psched->context[IDLE_TASK_INDEX][CTX_PTAB_ID]  = _ptabs_vaddr[0][x][y];
        psched->context[IDLE_TASK_INDEX][CTX_TTY_ID]   = 0;
        psched->context[IDLE_TASK_INDEX][CTX_LTID_ID]  = IDLE_TASK_INDEX;
        psched->context[IDLE_TASK_INDEX][CTX_VSID_ID]  = 0;
        psched->context[IDLE_TASK_INDEX][CTX_NORUN_ID] = 0;
        psched->context[IDLE_TASK_INDEX][CTX_SIG_ID]   = 0;
    }

    // HWI / PTI / WTI masks (up to 8 local processors)
    unsigned int hwi_mask[8] = {0,0,0,0,0,0,0,0};
    unsigned int pti_mask[8] = {0,0,0,0,0,0,0,0};
    unsigned int wti_mask[8] = {0,0,0,0,0,0,0,0};

    // scan local peripherals to get and check local XCU 
    mapping_periph_t*  xcu = NULL;
    unsigned int       min = cluster[cluster_id].periph_offset ;
    unsigned int       max = min + cluster[cluster_id].periphs ;

    for ( periph_id = min ; periph_id < max ; periph_id++ )
    {
        if( periph[periph_id].type == PERIPH_TYPE_XCU ) 
        {
            xcu = &periph[periph_id];

            // check nb_hwi_in
            if ( xcu->arg0 < xcu->irqs )
            {
                _printf("\n[BOOT ERROR] Not enough HWI inputs for XCU[%d,%d]"
                        " : nb_hwi = %d / nb_irqs = %d\n",
                         x , y , xcu->arg0 , xcu->irqs );
                _exit();
            }
            // check nb_pti_in
            if ( xcu->arg2 < nprocs )
            {
                _printf("\n[BOOT ERROR] Not enough PTI inputs for XCU[%d,%d]\n",
                         x, y );
                _exit();
            }
            // check nb_wti_in
            if ( xcu->arg1 < (4 * nprocs) )
            {
                _printf("\n[BOOT ERROR] Not enough WTI inputs for XCU[%d,%d]\n",
                        x, y );
                _exit();
            }
            // check nb_irq_out
            if ( xcu->channels < (nprocs * header->irq_per_proc) )
            {
                _printf("\n[BOOT ERROR] Not enough outputs for XCU[%d,%d]\n",
                        x, y );
                _exit();
            }
        }
    } 

    if ( xcu == NULL )
    {         
        _printf("\n[BOOT ERROR] missing XCU in cluster[%d,%d]\n", x , y );
        _exit();
    }

    // HWI interrupt vector definition
    // scan HWI connected to local XCU 
    // for round-robin allocation to local processors
    lpid = 0;
    for ( irq_id = xcu->irq_offset ;
          irq_id < xcu->irq_offset + xcu->irqs ;
          irq_id++ )
    {
        unsigned int type    = irq[irq_id].srctype;
        unsigned int srcid   = irq[irq_id].srcid;
        unsigned int isr     = irq[irq_id].isr & 0xFFFF;
        unsigned int channel = irq[irq_id].channel << 16;

        if ( (type != IRQ_TYPE_HWI) || (srcid > 31) )
        {
            _printf("\n[BOOT ERROR] Bad IRQ in cluster[%d,%d]\n", x, y );
            _exit();
        }

        // register entry in HWI interrupt vector
        _schedulers[x][y][lpid]->hwi_vector[srcid] = isr | channel;

        // update XCU HWI mask for P[x,y,lpid]
        hwi_mask[lpid] = hwi_mask[lpid] | (1<<srcid);

        lpid = (lpid + 1) % nprocs; 
    } // end for irqs

    // PTI interrupt vector definition
    // one PTI for TICK per processor
    for ( lpid = 0 ; lpid < nprocs ; lpid++ )
    {
        // register entry in PTI interrupt vector
        _schedulers[x][y][lpid]->pti_vector[lpid] = ISR_TICK;

        // update XCU PTI mask for P[x,y,lpid]
        pti_mask[lpid] = pti_mask[lpid] | (1<<lpid);
    }

    // WTI interrupt vector definition
    // 4 WTI per processor, first for WAKUP
    for ( lpid = 0 ; lpid < nprocs ; lpid++ )
    {
        // register WAKUP ISR in WTI interrupt vector
        _schedulers[x][y][lpid]->wti_vector[lpid] = ISR_WAKUP;

        // update XCU WTI mask for P[x,y,lpid] (4 entries per proc)
        wti_mask[lpid] = wti_mask[lpid] | (0x1<<(lpid                 ));
        wti_mask[lpid] = wti_mask[lpid] | (0x1<<(lpid + NB_PROCS_MAX  ));
        wti_mask[lpid] = wti_mask[lpid] | (0x1<<(lpid + 2*NB_PROCS_MAX));
        wti_mask[lpid] = wti_mask[lpid] | (0x1<<(lpid + 3*NB_PROCS_MAX));
    }

    // set the XCU masks for HWI / WTI / PTI interrupts 
    for ( lpid = 0 ; lpid < nprocs ; lpid++ )
    {
        unsigned int channel = lpid * IRQ_PER_PROCESSOR; 

        _xcu_set_mask( cluster_xy, channel, hwi_mask[lpid], IRQ_TYPE_HWI ); 
        _xcu_set_mask( cluster_xy, channel, wti_mask[lpid], IRQ_TYPE_WTI );
        _xcu_set_mask( cluster_xy, channel, pti_mask[lpid], IRQ_TYPE_PTI );
    }

    //////////////////////////////////////////////
    _simple_barrier_wait( &_barrier_all_clusters );
    //////////////////////////////////////////////

#if BOOT_DEBUG_SCHED
if ( cluster_xy == 0 ) boot_sched_irq_display();
_simple_barrier_wait( &_barrier_all_clusters );
#endif

    ///////////////////////////////////////////////////////////////////////////////
    // Step 2 : Initialise the tasks context. The context of task placed
    //          on  processor P must be stored in the scheduler of P.
    //          This require two nested loops: loop on the tasks, and loop 
    //          on the local processors. We complete the scheduler when the
    //          required placement fit one local processor.
    ///////////////////////////////////////////////////////////////////////////////

    for (vspace_id = 0; vspace_id < header->vspaces; vspace_id++) 
    {
        // We must set the PTPR depending on the vspace, because the start_vector 
        // and the stack address are defined in virtual space.
        _set_mmu_ptpr( (unsigned int)(_ptabs_paddr[vspace_id][x][y] >> 13) );

        // ctx_norun depends on the vspace active field
        unsigned int ctx_norun = (vspace[vspace_id].active == 0);

        // loop on the tasks in vspace (task_id is the global index in mapping)
        for (task_id = vspace[vspace_id].task_offset;
             task_id < (vspace[vspace_id].task_offset + vspace[vspace_id].tasks);
             task_id++) 
        {
            // get the required task placement coordinates [x,y,p]
            unsigned int req_x      = cluster[task[task_id].clusterid].x;
            unsigned int req_y      = cluster[task[task_id].clusterid].y;
            unsigned int req_p      = task[task_id].proclocid;                 

            // ctx_ptpr : page table physical base address (shifted by 13 bit)
            unsigned int ctx_ptpr = (_ptabs_paddr[vspace_id][req_x][req_y] >> 13);

            // ctx_ptab : page_table virtual base address
            unsigned int ctx_ptab = _ptabs_vaddr[vspace_id][req_x][req_y];

            // ctx_entry : Get the virtual address of the memory location containing
            // the task entry point : the start_vector is stored by GCC in the 
            // seg_data segment, and we must wait the .elf loading to get 
            // the entry point value...
            vseg_id = vspace[vspace_id].start_vseg_id;     
            unsigned int ctx_entry = vseg[vseg_id].vbase + (task[task_id].startid)*4;

            // ctx_sp :  Get the vseg containing the stack 
            vseg_id = task[task_id].stack_vseg_id;
            unsigned int ctx_sp = vseg[vseg_id].vbase + vseg[vseg_id].length;

            // get vspace thread index
            unsigned int thread_id = task[task_id].trdid;

            // loop on the local processors
            for ( lpid = 0 ; lpid < nprocs ; lpid++ )
            {
                if ( (x == req_x) && (y == req_y) && (req_p == lpid) )   // fit
                {
                    // pointer on selected scheduler
                    psched = _schedulers[x][y][lpid];

                    // get local task index in scheduler
                    unsigned int ltid = psched->tasks;

                    // update the "tasks" field in scheduler:
                    psched->tasks   = ltid + 1;

                    // initializes the task context 
                    psched->context[ltid][CTX_CR_ID]     = 0;
                    psched->context[ltid][CTX_SR_ID]     = GIET_SR_INIT_VALUE;
                    psched->context[ltid][CTX_SP_ID]     = ctx_sp;
                    psched->context[ltid][CTX_EPC_ID]    = ctx_entry;
                    psched->context[ltid][CTX_ENTRY_ID]  = ctx_entry;
                    psched->context[ltid][CTX_PTPR_ID]   = ctx_ptpr;
                    psched->context[ltid][CTX_PTAB_ID]   = ctx_ptab;
                    psched->context[ltid][CTX_LTID_ID]   = ltid;
                    psched->context[ltid][CTX_GTID_ID]   = task_id;
                    psched->context[ltid][CTX_TRDID_ID]  = thread_id;
                    psched->context[ltid][CTX_VSID_ID]   = vspace_id;
                    psched->context[ltid][CTX_NORUN_ID]  = ctx_norun;
                    psched->context[ltid][CTX_SIG_ID]    = 0;

                    psched->context[ltid][CTX_TTY_ID]    = 0xFFFFFFFF;
                    psched->context[ltid][CTX_CMA_FB_ID] = 0xFFFFFFFF;
                    psched->context[ltid][CTX_CMA_RX_ID] = 0xFFFFFFFF;
                    psched->context[ltid][CTX_CMA_TX_ID] = 0xFFFFFFFF;
                    psched->context[ltid][CTX_NIC_RX_ID] = 0xFFFFFFFF;
                    psched->context[ltid][CTX_NIC_TX_ID] = 0xFFFFFFFF;
                    psched->context[ltid][CTX_TIM_ID]    = 0xFFFFFFFF;
                    psched->context[ltid][CTX_HBA_ID]    = 0xFFFFFFFF;

                    // update task ltid field in the mapping
                    task[task_id].ltid = ltid;

#if BOOT_DEBUG_SCHED
_printf("\nTask %s in vspace %s allocated to P[%d,%d,%d]\n"
        " - ctx[LTID]  = %d\n"
        " - ctx[SR]    = %x\n"
        " - ctx[SP]    = %x\n"
        " - ctx[ENTRY] = %x\n"
        " - ctx[PTPR]  = %x\n"
        " - ctx[PTAB]  = %x\n"
        " - ctx[VSID]  = %d\n"
        " - ctx[TRDID] = %d\n"
        " - ctx[NORUN] = %x\n"
        " - ctx[SIG]   = %x\n",
        task[task_id].name,
        vspace[vspace_id].name,
        x, y, lpid,
        psched->context[ltid][CTX_LTID_ID],
        psched->context[ltid][CTX_SR_ID],
        psched->context[ltid][CTX_SP_ID],
        psched->context[ltid][CTX_ENTRY_ID],
        psched->context[ltid][CTX_PTPR_ID],
        psched->context[ltid][CTX_PTAB_ID],
        psched->context[ltid][CTX_VSID_ID],
        psched->context[ltid][CTX_TRDID_ID],
        psched->context[ltid][CTX_NORUN_ID],
        psched->context[ltid][CTX_SIG_ID] );
#endif
                } // end if FIT
            } // end for loop on local procs
        } // end loop on tasks
    } // end loop on vspaces
} // end boot_scheduler_init()



//////////////////////////////////////////////////////////////////////////////////
// This function loads the map.bin file from block device.
//////////////////////////////////////////////////////////////////////////////////
void boot_mapping_init()
{
    // load map.bin file into buffer
    if ( _fat_load_no_cache( "map.bin",
                             SEG_BOOT_MAPPING_BASE,
                             SEG_BOOT_MAPPING_SIZE ) )
    {
        _printf("\n[BOOT ERROR] : map.bin file not found \n");
        _exit();
    }

    // check mapping signature, number of clusters, number of vspaces  
    mapping_header_t * header = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    if ( (header->signature != IN_MAPPING_SIGNATURE) ||
         (header->x_size    != X_SIZE)               || 
         (header->y_size    != Y_SIZE)               ||
         (header->vspaces   > GIET_NB_VSPACE_MAX)    )
    {
        _printf("\n[BOOT ERROR] Illegal mapping : signature = %x\n", header->signature );
        _exit();
    }

#if BOOT_DEBUG_MAPPING
unsigned int  line;
unsigned int* pointer = (unsigned int*)SEG_BOOT_MAPPING_BASE;
_printf("\n[BOOT] First block of mapping\n");
for ( line = 0 ; line < 8 ; line++ )
{
    _printf(" | %X | %X | %X | %X | %X | %X | %X | %X |\n",
            *(pointer + 0),
            *(pointer + 1),
            *(pointer + 2),
            *(pointer + 3),
            *(pointer + 4),
            *(pointer + 5),
            *(pointer + 6),
            *(pointer + 7) );

    pointer = pointer + 8;
}
#endif

} // end boot_mapping_init()


///////////////////////////////////////////////////
void boot_dma_copy( unsigned int        cluster_xy,      
                    unsigned long long  dst_paddr,
                    unsigned long long  src_paddr, 
                    unsigned int        size )   
{
    // size must be multiple of 64 bytes
    if ( size & 0x3F ) size = (size & (~0x3F)) + 0x40;

    unsigned int mode = MODE_DMA_NO_IRQ;

    unsigned int src     = 0;
    unsigned int src_lsb = (unsigned int)src_paddr;
    unsigned int src_msb = (unsigned int)(src_paddr>>32);
    
    unsigned int dst     = 1;
    unsigned int dst_lsb = (unsigned int)dst_paddr;
    unsigned int dst_msb = (unsigned int)(dst_paddr>>32);

    // initializes src channel
    _mwr_set_channel_register( cluster_xy , src , MWR_CHANNEL_MODE       , mode );
    _mwr_set_channel_register( cluster_xy , src , MWR_CHANNEL_SIZE       , size );
    _mwr_set_channel_register( cluster_xy , src , MWR_CHANNEL_BUFFER_LSB , src_lsb );
    _mwr_set_channel_register( cluster_xy , src , MWR_CHANNEL_BUFFER_MSB , src_msb );
    _mwr_set_channel_register( cluster_xy , src , MWR_CHANNEL_RUNNING    , 1 );

    // initializes dst channel
    _mwr_set_channel_register( cluster_xy , dst , MWR_CHANNEL_MODE       , mode );
    _mwr_set_channel_register( cluster_xy , dst , MWR_CHANNEL_SIZE       , size );
    _mwr_set_channel_register( cluster_xy , dst , MWR_CHANNEL_BUFFER_LSB , dst_lsb );
    _mwr_set_channel_register( cluster_xy , dst , MWR_CHANNEL_BUFFER_MSB , dst_msb );
    _mwr_set_channel_register( cluster_xy , dst , MWR_CHANNEL_RUNNING    , 1 );

    // start CPY coprocessor (write non-zero value into config register)
    _mwr_set_coproc_register( cluster_xy, 0 , 1 );

    // poll dst channel status register to detect completion
    unsigned int status;
    do
    {
        status = _mwr_get_channel_register( cluster_xy , dst , MWR_CHANNEL_STATUS );
    } while ( status == MWR_CHANNEL_BUSY );

    if ( status )
    {
        _printf("\n[BOOT ERROR] in boot_dma_copy()\n");
        _exit();
    }  
  
    // stop CPY coprocessor and DMA channels
    _mwr_set_channel_register( cluster_xy , src , MWR_CHANNEL_RUNNING    , 0 );
    _mwr_set_channel_register( cluster_xy , dst , MWR_CHANNEL_RUNNING    , 0 );
    _mwr_set_coproc_register ( cluster_xy , 0 , 0 );

}  // end boot_dma_copy()

//////////////////////////////////////////////////////////////////////////////////
// This function load all loadable segments contained in the .elf file identified 
// by the "pathname" argument. Some loadable segments can be copied in several
// clusters: same virtual address but different physical addresses.  
// - It open the file.
// - It loads the complete file in the dedicated _boot_elf_buffer.
// - It copies each loadable segments  at the virtual address defined in 
//   the .elf file, making several copies if the target vseg is not local.
// - It closes the file.
// This function is supposed to be executed by all processors[x,y,0].
//
// Note: We must use physical addresses to reach the destination buffers that
// can be located in remote clusters. We use either a _physical_memcpy(), 
// or a _dma_physical_copy() if DMA is available.
//////////////////////////////////////////////////////////////////////////////////
void load_one_elf_file( unsigned int is_kernel,     // kernel file if non zero
                        char*        pathname,
                        unsigned int vspace_id )    // to scan the proper vspace
{
    mapping_header_t  * header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t  * vspace  = _get_vspace_base(header);
    mapping_vseg_t    * vseg    = _get_vseg_base(header);

    unsigned int procid = _get_procid();
    unsigned int cxy    = procid >> P_WIDTH;
    unsigned int x      = cxy >> Y_WIDTH;
    unsigned int y      = cxy & ((1<<Y_WIDTH)-1);
    unsigned int p      = procid & ((1<<P_WIDTH)-1);

#if BOOT_DEBUG_ELF
_printf("\n[DEBUG BOOT_ELF] load_one_elf_file() : P[%d,%d,%d] enters for %s\n",
        x , y , p , pathname );
#endif

    Elf32_Ehdr* elf_header_ptr = NULL;  //  avoid a warning

    // only P[0,0,0] load file 
    if ( (cxy == 0) && (p == 0) )
    {
        if ( _fat_load_no_cache( pathname,
                                 (unsigned int)_boot_elf_buffer,
                                 GIET_ELF_BUFFER_SIZE ) )
        {
            _printf("\n[BOOT ERROR] in load_one_elf_file() : %s\n", pathname );
            _exit();
        }

        // Check ELF Magic Number in ELF header
        Elf32_Ehdr* ptr = (Elf32_Ehdr*)_boot_elf_buffer;

        if ( (ptr->e_ident[EI_MAG0] != ELFMAG0) ||
             (ptr->e_ident[EI_MAG1] != ELFMAG1) ||
             (ptr->e_ident[EI_MAG2] != ELFMAG2) ||
             (ptr->e_ident[EI_MAG3] != ELFMAG3) )
        {
            _printf("\n[BOOT ERROR] load_one_elf_file() : %s not ELF format\n",
                    pathname );
            _exit();
        }

#if BOOT_DEBUG_ELF
_printf("\n[DEBUG BOOT_ELF] load_one_elf_file() : P[%d,%d,%d] load %s at cycle %d\n", 
        x , y , p , pathname , _get_proctime() );
#endif

    } // end if P[0,0,0]

    //////////////////////////////////////////////
    _simple_barrier_wait( &_barrier_all_clusters );
    //////////////////////////////////////////////

    // Each processor P[x,y,0] copy replicated segments in cluster[x,y]
    elf_header_ptr = (Elf32_Ehdr*)_boot_elf_buffer;

    // get program header table pointer
    unsigned int offset = elf_header_ptr->e_phoff;
    if( offset == 0 )
    {
        _printf("\n[BOOT ERROR] load_one_elf_file() : file %s "
                "does not contain loadable segment\n", pathname );
        _exit();
    }

    Elf32_Phdr* elf_pht_ptr = (Elf32_Phdr*)(_boot_elf_buffer + offset);

    // get number of segments
    unsigned int nsegments   = elf_header_ptr->e_phnum;

    // First loop on loadable segments in the .elf file
    unsigned int seg_id;
    for (seg_id = 0 ; seg_id < nsegments ; seg_id++)
    {
        if(elf_pht_ptr[seg_id].p_type == PT_LOAD)
        {
            // Get segment attributes
            unsigned int seg_vaddr  = elf_pht_ptr[seg_id].p_vaddr;
            unsigned int seg_offset = elf_pht_ptr[seg_id].p_offset;
            unsigned int seg_filesz = elf_pht_ptr[seg_id].p_filesz;
            unsigned int seg_memsz  = elf_pht_ptr[seg_id].p_memsz;

            if( seg_memsz != seg_filesz )
            {
                _printf("\n[BOOT ERROR] load_one_elf_file() : segment at vaddr = %x\n"
                        " in file %s has memsize = %x / filesize = %x \n"
                        " check that all global variables are in data segment\n", 
                        seg_vaddr, pathname , seg_memsz , seg_filesz );
                _exit();
            }

            unsigned int src_vaddr = (unsigned int)_boot_elf_buffer + seg_offset;

            // search all vsegs matching the virtual address
            unsigned int vseg_first;
            unsigned int vseg_last;
            unsigned int vseg_id;
            unsigned int found = 0;
            if ( is_kernel )
            {
                vseg_first = 0;
                vseg_last  = header->globals;
            }
            else
            {
                vseg_first = vspace[vspace_id].vseg_offset;
                vseg_last  = vseg_first + vspace[vspace_id].vsegs;
            }

            // Second loop on vsegs in the mapping
            for ( vseg_id = vseg_first ; vseg_id < vseg_last ; vseg_id++ )
            {
                if ( seg_vaddr == vseg[vseg_id].vbase )  // matching 
                {
                    found = 1;

                    // get destination buffer physical address, size, coordinates 
                    paddr_t      seg_paddr  = vseg[vseg_id].pbase;
                    unsigned int seg_size   = vseg[vseg_id].length;
                    unsigned int cluster_xy = (unsigned int)(seg_paddr>>32);
                    unsigned int cx         = cluster_xy >> Y_WIDTH;
                    unsigned int cy         = cluster_xy & ((1<<Y_WIDTH)-1);

                    // check vseg size
                    if ( seg_size < seg_filesz )
                    {
                        _printf("\n[BOOT ERROR] in load_one_elf_file() : vseg %s "
                                "is too small for segment %x\n"
                                "  file = %s / vseg_size = %x / seg_file_size = %x\n",
                                vseg[vseg_id].name , seg_vaddr , pathname,
                                seg_size , seg_filesz );
                        _exit();
                    }

                    // P[x,y,0] copy the segment from boot buffer in cluster[0,0] 
                    // to destination buffer in cluster[x,y], using DMA if available
                    if ( (cx == x) && (cy == y) )
                    {
                        if( USE_MWR_CPY )
                        {
                            boot_dma_copy( cluster_xy,  // DMA in cluster[x,y]       
                                           seg_paddr,
                                           (paddr_t)src_vaddr, 
                                           seg_filesz );   
#if BOOT_DEBUG_ELF
_printf("\n[DEBUG BOOT_ELF] load_one_elf_file() : DMA[%d,%d] copy segment %d :\n"
        "  vaddr = %x / size = %x / paddr = %l\n",
        x , y , seg_id , seg_vaddr , seg_memsz , seg_paddr );
#endif
                        }
                        else
                        {
                            _physical_memcpy( seg_paddr,            // dest paddr
                                              (paddr_t)src_vaddr,   // source paddr
                                              seg_filesz );         // size
#if BOOT_DEBUG_ELF
_printf("\n[DEBUG BOOT_ELF] load_one_elf_file() : P[%d,%d,%d] copy segment %d :\n"
        "  vaddr = %x / size = %x / paddr = %l\n",
        x , y , p , seg_id , seg_vaddr , seg_memsz , seg_paddr );
#endif
                        }
                    }
                }
            }  // end for vsegs 

            // check at least one matching vseg
            if ( found == 0 )
            {
                _printf("\n[BOOT ERROR] in load_one_elf_file() : vseg for loadable "
                        "segment %x in file %s not found "
                        "check consistency between the .py and .ld files\n",
                        seg_vaddr, pathname );
                _exit();
            }
        }
    }  // end for loadable segments

    //////////////////////////////////////////////
    _simple_barrier_wait( &_barrier_all_clusters );
    //////////////////////////////////////////////

    // only P[0,0,0] signals completion
    if ( (cxy == 0) && (p == 0) )
    {
        _printf("\n[BOOT] File %s loaded at cycle %d\n", 
                pathname , _get_proctime() );
    }

} // end load_one_elf_file()


/////i////////////////////////////////////////////////////////////////////////////////
// This function uses the map.bin data structure to load the "kernel.elf" file
// as well as the various "application.elf" files into memory.
// - The "preloader.elf" file is not loaded, because it has been burned in the ROM.
// - The "boot.elf" file is not loaded, because it has been loaded by the preloader.
// This function scans all vsegs defined in the map.bin data structure to collect
// all .elf files pathnames, and calls the load_one_elf_file() for each .elf file.
// As the code can be replicated in several vsegs, the same code can be copied 
// in one or several clusters by the load_one_elf_file() function.
//////////////////////////////////////////////////////////////////////////////////////
void boot_elf_load()
{
    mapping_header_t* header = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_vspace_t* vspace = _get_vspace_base( header );
    mapping_vseg_t*   vseg   = _get_vseg_base( header );

    unsigned int      vspace_id;
    unsigned int      vseg_id;
    unsigned int      found;

    // Scan all global vsegs to find the pathname to the kernel.elf file
    found = 0;
    for( vseg_id = 0 ; vseg_id < header->globals ; vseg_id++ )
    {
        if(vseg[vseg_id].type == VSEG_TYPE_ELF) 
        {   
            found = 1;
            break;
        }
    }

    // We need one kernel.elf file
    if (found == 0)
    {
        _printf("\n[BOOT ERROR] boot_elf_load() : kernel.elf file not found\n");
        _exit();
    }

    // Load the kernel 
    load_one_elf_file( 1,                           // kernel file
                       vseg[vseg_id].binpath,       // file pathname
                       0 );                         // vspace 0

    // loop on the vspaces, scanning all vsegs in the vspace,
    // to find the pathname of the .elf file associated to the vspace.
    for( vspace_id = 0 ; vspace_id < header->vspaces ; vspace_id++ )
    {
        // loop on the private vsegs
        unsigned int found = 0;
        for (vseg_id = vspace[vspace_id].vseg_offset;
             vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs);
             vseg_id++) 
        {
            if(vseg[vseg_id].type == VSEG_TYPE_ELF) 
            {   
                found = 1;
                break;
            }
        }

        // We want one .elf file per vspace
        if (found == 0)
        {
            _printf("\n[BOOT ERROR] boot_elf_load() : "
                    ".elf file not found for vspace %s\n", vspace[vspace_id].name );
            _exit();
        }

        load_one_elf_file( 0,                          // not a kernel file
                           vseg[vseg_id].binpath,      // file pathname
                           vspace_id );                // vspace index

    }  // end for vspaces

} // end boot_elf_load()


/////////////////////////////////////////////////////////////////////////////////
// This function is executed in parallel by all processors[x][y][0].
// It initialises the physical memory allocator in each cluster containing 
// a RAM pseg.
/////////////////////////////////////////////////////////////////////////////////
void boot_pmem_init( unsigned int cx,
                     unsigned int cy ) 
{
    mapping_header_t*  header     = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t* cluster    = _get_cluster_base(header);
    mapping_pseg_t*    pseg       = _get_pseg_base(header);

    unsigned int pseg_id;
    unsigned int procid     = _get_procid();
    unsigned int lpid       = procid & ((1<<P_WIDTH)-1);

    if( lpid )
    {
        _printf("\n[BOOT ERROR] boot_pmem_init() : "
        "P[%d][%d][%d] should not execute it\n", cx, cy, lpid );
        _exit();
    }   

    // scan the psegs in local cluster to find  pseg of type RAM
    unsigned int found      = 0;
    unsigned int cluster_id = cx * Y_SIZE + cy;
    unsigned int pseg_min   = cluster[cluster_id].pseg_offset;
    unsigned int pseg_max   = pseg_min + cluster[cluster_id].psegs;
    for ( pseg_id = pseg_min ; pseg_id < pseg_max ; pseg_id++ )
    {
        if ( pseg[pseg_id].type == PSEG_TYPE_RAM )
        {
            unsigned int base = (unsigned int)pseg[pseg_id].base;
            unsigned int size = (unsigned int)pseg[pseg_id].length;
            _pmem_alloc_init( cx, cy, base, size );
            found = 1;

#if BOOT_DEBUG_PT
_printf("\n[BOOT] pmem allocator initialised in cluster[%d][%d]"
        " : base = %x / size = %x\n", cx , cy , base , size );
#endif
            break;
        }
    }

    if ( found == 0 )
    {
        _printf("\n[BOOT ERROR] boot_pmem_init() : no RAM in cluster[%d][%d]\n",
                cx , cy );
        _exit();
    }   
} // end boot_pmem_init()
 
/////////////////////////////////////////////////////////////////////////
// This function is the entry point of the boot code for all processors.
/////////////////////////////////////////////////////////////////////////
void boot_init() 
{

    unsigned int       gpid       = _get_procid();
    unsigned int       cx         = gpid >> (Y_WIDTH + P_WIDTH);
    unsigned int       cy         = (gpid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    unsigned int       lpid       = gpid & ((1 << P_WIDTH) -1);

    //////////////////////////////////////////////////////////
    // Phase ONE : only P[0][0][0] execute it
    //////////////////////////////////////////////////////////
    if ( gpid == 0 )    
    {
        unsigned int cid;  // index for loop on clusters 

        // initialises the TTY0 spin lock
        _spin_lock_init( &_tty0_spin_lock );

        _printf("\n[BOOT] P[0,0,0] starts at cycle %d\n", _get_proctime() );

        // initialise the MMC locks array
        _mmc_boot_mode = 1;
        _mmc_init_locks();

        // initialises the IOC peripheral
        if      ( USE_IOC_BDV != 0 ) _bdv_init();
        else if ( USE_IOC_HBA != 0 ) _hba_init();
        else if ( USE_IOC_SDC != 0 ) _sdc_init();
        else if ( USE_IOC_RDK == 0 )
        {
            _printf("\n[BOOT ERROR] boot_init() : no IOC peripheral\n");
            _exit();
        }

        // initialises the FAT
        _fat_init( 0 );          // don't use Inode-Tree, Fat-Cache, etc.

        _printf("\n[BOOT] FAT initialised at cycle %d\n", _get_proctime() );

        // Load the map.bin file into memory 
        boot_mapping_init();

        mapping_header_t*  header     = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
        mapping_cluster_t* cluster    = _get_cluster_base(header);

        _printf("\n[BOOT] Mapping %s loaded at cycle %d\n",
                header->name , _get_proctime() );

        // initialises the barrier for all clusters containing processors
        unsigned int nclusters = 0;
        for ( cid = 0 ; cid < X_SIZE*Y_SIZE ; cid++ )
        {
            if ( cluster[cid].procs ) nclusters++ ;
        } 

        _simple_barrier_init( &_barrier_all_clusters , nclusters );

        // wake up all processors P[x][y][0]
        for ( cid = 1 ; cid < X_SIZE*Y_SIZE ; cid++ ) 
        {
            unsigned int x          = cluster[cid].x;
            unsigned int y          = cluster[cid].y;
            unsigned int cluster_xy = (x << Y_WIDTH) + y;

            if ( cluster[cid].procs ) 
            {
                unsigned long long paddr = (((unsigned long long)cluster_xy)<<32) +
                                           SEG_XCU_BASE+XCU_REG( XCU_WTI_REG , 0 );

                _physical_write( paddr , (unsigned int)boot_entry );
            }
        }

        _printf("\n[BOOT] Processors P[x,y,0] start at cycle %d\n",
                _get_proctime() );
    }

    /////////////////////////////////////////////////////////////////
    // Phase TWO : All processors P[x][y][0] execute it in parallel
    /////////////////////////////////////////////////////////////////
    if( lpid == 0 )
    {
        // Initializes physical memory allocator in cluster[cx][cy]
        boot_pmem_init( cx , cy );

        // Build page table in cluster[cx][cy]
        boot_ptab_init( cx , cy );

        //////////////////////////////////////////////
        _simple_barrier_wait( &_barrier_all_clusters );
        //////////////////////////////////////////////

        // P[0][0][0] complete page tables with vsegs 
        // mapped in clusters without processors 
        if ( gpid == 0 )   
        {
            // complete page tables initialisation
            boot_ptab_extend();

            _printf("\n[BOOT] Physical memory allocators and page tables"
                    " initialized at cycle %d\n", _get_proctime() );
        }

        //////////////////////////////////////////////
        _simple_barrier_wait( &_barrier_all_clusters );
        //////////////////////////////////////////////

        // All processors P[x,y,0] activate MMU (using local PTAB) 
        _set_mmu_ptpr( (unsigned int)(_ptabs_paddr[0][cx][cy]>>13) );
        _set_mmu_mode( 0xF );
        
        // Each processor P[x,y,0] initialises all schedulers in cluster[x,y]
        boot_scheduler_init( cx , cy );

        // Each processor P[x][y][0] initialises its CP0_SCHED register
        _set_sched( (unsigned int)_schedulers[cx][cy][0] );

        //////////////////////////////////////////////
        _simple_barrier_wait( &_barrier_all_clusters );
        //////////////////////////////////////////////

        if ( gpid == 0 ) 
        {
            _printf("\n[BOOT] Schedulers initialised at cycle %d\n", 
                    _get_proctime() );
        }

        // All processor P[x,y,0] contributes to load .elf files into clusters.
        boot_elf_load();

        //////////////////////////////////////////////
        _simple_barrier_wait( &_barrier_all_clusters );
        //////////////////////////////////////////////
        
        // Each processor P[x][y][0] wake up other processors in same cluster
        mapping_header_t*  header     = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
        mapping_cluster_t* cluster    = _get_cluster_base(header);
        unsigned int       cluster_xy = (cx << Y_WIDTH) + cy;
        unsigned int       cluster_id = (cx * Y_SIZE) + cy;
        unsigned int p;
        for ( p = 1 ; p < cluster[cluster_id].procs ; p++ )
        {
            _xcu_send_wti( cluster_xy , p , (unsigned int)boot_entry );
        }

        // only P[0][0][0] makes display
        if ( gpid == 0 )
        {    
            _printf("\n[BOOT] All processors start at cycle %d\n",
                    _get_proctime() );
        }
    }
    // All other processors activate MMU (using local PTAB)
    if ( lpid != 0 )
    {
        _set_mmu_ptpr( (unsigned int)(_ptabs_paddr[0][cx][cy]>>13) );
        _set_mmu_mode( 0xF );
    }

    // All processors set CP0_SCHED register 
    _set_sched( (unsigned int)_schedulers[cx][cy][lpid] );

    // All processors reset BEV bit in SR to use GIET_VM exception handler 
    _set_sr( 0 );

    // Each processor get kernel entry virtual address
    unsigned int kernel_entry = (unsigned int)&kernel_init_vbase;

#if BOOT_DEBUG_ELF
_printf("\n[DEBUG BOOT_ELF] P[%d,%d,%d] exit boot & jump to %x at cycle %d\n",
        cx, cy, lpid, kernel_entry , _get_proctime() );
#endif

    // All processors jump to kernel_init 
    asm volatile( "jr   %0" ::"r"(kernel_entry) );

} // end boot_init()


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

