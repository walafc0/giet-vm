////////////////////////////////////////////////////////////////////////////
// File     : mapping_info.h
// Date     : 01/04/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////
// The MAPPING_INFO data structure, used by the GIET_VM kernel contains:
// 
// 1) a description of a clusterized hardware architecture.
// The number of cluster is variable (can be one). 
// The number of processors per cluster is variable (can be zero). 
// The number of peripherals per cluster is variable (can be zero),
// The number of physical memory bank per cluster is variable.
//
// 2/ a description of the applications (called vspaces) to be - statically - 
// launched on the platform. The number of parallel tasks per application is
// variable (can be one). Each vspace contains a variable number 
// of virtual segments (called vsegs). 
//
// 3/ the mapping directives: both tasks on processors, and software objects 
// (vsegs) on the physical memory banks (psegs).
//
// The mapping_info data structure is organised as the concatenation of
// a fixed size header, and 8 variable size arrays:
//
// - mapping_cluster_t  cluster[]  
// - mapping_pseg_t     pseg[]       
// - mapping_vspace_t   vspace[]  
// - mapping_vseg_t     vseg[]     
// - mapping_task_t     task[]  
// - mapping_proc_t     proc[]  
// - mapping_irq_t      irq[]   
// - mapping_periph_t   periph[]
//
// It is intended to be stored in memory in the seg_boot_mapping segment. 
////////////////////////////////////////////////////////////////////////////

#ifndef _MAPPING_INFO_H_
#define _MAPPING_INFO_H_

#define MAPPING_HEADER_SIZE   sizeof(mapping_header_t)
#define MAPPING_CLUSTER_SIZE  sizeof(mapping_cluster_t)
#define MAPPING_VSPACE_SIZE   sizeof(mapping_vspace_t)
#define MAPPING_VSEG_SIZE     sizeof(mapping_vseg_t)
#define MAPPING_PSEG_SIZE     sizeof(mapping_pseg_t)
#define MAPPING_TASK_SIZE     sizeof(mapping_task_t)
#define MAPPING_PROC_SIZE     sizeof(mapping_proc_t)
#define MAPPING_IRQ_SIZE      sizeof(mapping_irq_t)
#define MAPPING_PERIPH_SIZE   sizeof(mapping_periph_t)

#define C_MODE_MASK  0b1000   // cacheable
#define X_MODE_MASK  0b0100   // executable
#define W_MODE_MASK  0b0010   // writable
#define U_MODE_MASK  0b0001   // user access

#define IN_MAPPING_SIGNATURE    0xDACE2014
#define OUT_MAPPING_SIGNATURE   0xBABEF00D

typedef unsigned long long  paddr_t;

enum vsegType 
{
    VSEG_TYPE_ELF      = 0,  // loadable code/data object of elf files
    VSEG_TYPE_BLOB     = 1,  // loadable blob object
    VSEG_TYPE_PTAB     = 2,  // page table 
    VSEG_TYPE_PERI     = 3,  // hardware component
    VSEG_TYPE_BUFFER   = 4,  // no initialization object (stacks...)
    VSEG_TYPE_SCHED    = 5,  // scheduler 
    VSEG_TYPE_HEAP     = 6,  // heap 
};

enum irqType
{
    IRQ_TYPE_HWI = 0,        // HARD in map.xml file
    IRQ_TYPE_WTI = 1,        // SOFT in map.xml file,
    IRQ_TYPE_PTI = 2,        // TIME in map.xml file,
};


enum psegType 
{
    PSEG_TYPE_RAM  = 0,
    PSEG_TYPE_PERI = 2,
};


////////////////////////////////////////////////////////////////////
// These enum must be coherent with the values in
// xml_driver.c / xml_parser.c / mapping.py
///////////////////////////////////////////////////////////////////
enum periphTypes
{
    PERIPH_TYPE_CMA       = 0,
    PERIPH_TYPE_DMA       = 1,
    PERIPH_TYPE_FBF       = 2,
    PERIPH_TYPE_IOB       = 3,
    PERIPH_TYPE_IOC       = 4,
    PERIPH_TYPE_MMC       = 5,
    PERIPH_TYPE_MWR       = 6,
    PERIPH_TYPE_NIC       = 7,
    PERIPH_TYPE_ROM       = 8,
    PERIPH_TYPE_SIM       = 9,
    PERIPH_TYPE_TIM       = 10,
    PERIPH_TYPE_TTY       = 11,
    PERIPH_TYPE_XCU       = 12,
    PERIPH_TYPE_PIC       = 13,
    PERIPH_TYPE_DROM      = 14,

    PERIPH_TYPE_MAX_VALUE = 15,
};

enum iocTypes 
{
    IOC_SUBTYPE_BDV       = 0,
    IOC_SUBTYPE_HBA       = 1,
    IOC_SUBTYPE_SDC       = 2,
    IOC_SUBTYPE_SPI       = 3,
};

enum mwrTypes
{
    MWR_SUBTYPE_GCD       = 0,
    MWR_SUBTYPE_DCT       = 1,
    MWR_SUBTYPE_CPY       = 2,
};

//////////////////////////////////////////////////////////
// This enum must be coherent with the values in 
// mwr_driver.h                
//////////////////////////////////////////////////////////
enum MwmrDmaModes
{
    MODE_MWMR             = 0,
    MODE_DMA_IRQ          = 1,
    MODE_DMA_NO_IRQ       = 2,
};

////////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_header_s 
{
    unsigned int    signature;       // must contain IN_MAPPING_SIGNATURE
    unsigned int    x_size;          // actual number of clusters in a row
    unsigned int    y_size;          // actual number of clusters in a column
    unsigned int    x_width;         // number of bits to encode x coordinate 
    unsigned int    y_width;         // number of bits to encode y coordinate 
    unsigned int    x_io;            // x coordinate for cluster_io_ext
    unsigned int    y_io;            // y coordinate for cluster_io_ext
    unsigned int    irq_per_proc;    // number of IRQ per processor
    unsigned int    use_ram_disk;    // does not use IOC peripheral if non zero
    unsigned int    globals;         // total number of global vsegs
    unsigned int    vspaces;         // total number of virtual spaces
    unsigned int    psegs;           // total number of physical segments 
    unsigned int    vsegs;           // total number of virtual segments
    unsigned int    tasks;           // total number of tasks 
    unsigned int    procs;           // total number of processors 
    unsigned int    irqs;            // total number of irqs 
    unsigned int    periphs;         // total number of peripherals 
    char name[64];                   // mapping name
} mapping_header_t;


/////////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_cluster_s 
{
    unsigned int    x;               // x coordinate
    unsigned int    y;               // y coordinate

    unsigned int    psegs;           // number of psegs in cluster
    unsigned int    pseg_offset;     // global index of first pseg in cluster

    unsigned int    procs;           // number of processors in cluster
    unsigned int    proc_offset;     // global index of first proc in cluster
 
    unsigned int    periphs;         // number of peripherals in cluster
    unsigned int    periph_offset;   // global index of first coproc in cluster
} mapping_cluster_t;


////////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_vspace_s 
{
    char            name[32];        // virtual space name
    unsigned int    start_vseg_id;   // vseg containing start vector index
    unsigned int    vsegs;           // number of vsegs in vspace
    unsigned int    tasks;           // number of tasks in vspace
    unsigned int    vseg_offset;     // global index of first vseg in vspace 
    unsigned int    task_offset;     // global index of first task in vspace
    unsigned int    active;          // always active if non zero
} mapping_vspace_t;


//////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_vseg_s 
{
    char            name[32];        // vseg name (unique in vspace)
    char            binpath[64];     // path for the binary code (if required)
    unsigned int    vbase;           // base address in virtual space
    paddr_t         pbase;           // base address in physical space
    unsigned int    length;          // size (bytes)
    unsigned int    psegid;          // physical segment global index
    unsigned int    mode;            // C-X-W-U flags
    unsigned int    type;            // vseg type
    char            mapped;          // mapped if non zero
    char            ident;           // identity mapping if non zero
    char            local;           // only mapped in the local PTAB
    char            big;             // to be mapped in a big physical page
} mapping_vseg_t;


//////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_pseg_s  
{
    char            name[32];        // pseg name (unique in a cluster)
    paddr_t         base;            // base address in physical space
    paddr_t         length;          // size (bytes)
    unsigned int    type;            // RAM / PERI
    unsigned int    clusterid;       // global index in clusters set
    unsigned int    next_vseg;       // linked list of vsegs mapped on pseg
} mapping_pseg_t;


//////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_task_s 
{
    char            name[32];        // task name (unique in vspace)
    unsigned int    clusterid;       // global index in clusters set
    unsigned int    proclocid;       // processor local index (inside cluster)
    unsigned int    trdid;           // thread index in vspace
    unsigned int    stack_vseg_id;   // global index for vseg containing stack
    unsigned int    heap_vseg_id;    // global index for vseg containing heap
    unsigned int    startid;         // index in start_vector 
    unsigned int    ltid;            // task index in scheduler (dynamically defined)
} mapping_task_t;


//////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_proc_s 
{
    unsigned int    index;           // processor local index (in cluster)
} mapping_proc_t;


////////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_periph_s 
{
    unsigned int    type;            // legal values defined above
    unsigned int    subtype;         // periph specialization
    unsigned int    psegid;          // pseg index in cluster
    unsigned int    channels;        // number of channels
    unsigned int    arg0;            // semantic depends on peripheral type
    unsigned int    arg1;            // semantic depends on peripheral type
    unsigned int    arg2;            // semantic depends on peripheral type
    unsigned int    arg3;            // semantic depends on peripheral type
    unsigned int    irqs;            // number of input IRQs (for XCU or PIC)
    unsigned int    irq_offset;      // index of first IRQ 
} mapping_periph_t;


/////////////////////////////////////////////////////
typedef struct __attribute__((packed))  mapping_irq_s 
{
    unsigned int    srctype;         // source IRQ type (HWI / WTI / PTI)
    unsigned int    srcid;           // source IRQ index (for XCU/PIC)
    unsigned int    isr;             // ISR type (defined in irq_handler.h)
    unsigned int    channel;         // channel index (for multi-channels ISR)
    unsigned int    dest_xy;         // destination cluster (set by GIET_VM)
    unsigned int    dest_id;         // destination port (set by GIET_VM)
} mapping_irq_t; 


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

