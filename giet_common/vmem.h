///////////////////////////////////////////////////////////////////////////////////
// File     : vmem.h
// Date     : 01/07/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The vmem.c and vmem.h files are part ot the GIET-VM nano kernel.
// They define  the data structures implementing the page tables,
// and the function used for VPN to PPN translation.
///////////////////////////////////////////////////////////////////////////////////
// The virtual address format is 32 bits: structures in 3 fields:
//             |  11  |  9   |   12   |
//             | IX1  | IX2  | OFFSET |
// - The IX1 field is the index in the first level page table
// - The IX2 field is the index in the second level page table
// - The |IX1|IX2\ concatenation defines the VPN (Virtual Page Number)
///////////////////////////////////////////////////////////////////////////////////

#ifndef _VMEM_H_
#define _VMEM_H_

/////////////////////////////////////////////////////////////////////////////////////
// Page Table sizes definition
/////////////////////////////////////////////////////////////////////////////////////

#define PT1_SIZE    8192
#define PT2_SIZE    4096

#define VPN_MASK    0xFFFFF000
#define BPN_MASK    0xFFE00000

/////////////////////////////////////////////////////////////////////////////////////
// PTE flags masks definition 
/////////////////////////////////////////////////////////////////////////////////////

#define PTE_V  0x80000000
#define PTE_T  0x40000000
#define PTE_L  0x20000000
#define PTE_R  0x10000000
#define PTE_C  0x08000000
#define PTE_W  0x04000000
#define PTE_X  0x02000000
#define PTE_U  0x01000000
#define PTE_G  0x00800000
#define PTE_D  0x00400000

///////////////////////////////////////////////////////////////////////////////////
// MMU error codes definition 
///////////////////////////////////////////////////////////////////////////////////

#define MMU_ERR_PT1_UNMAPPED         0x001 // Page fault on Table1 (invalid PTE) 
#define MMU_ERR_PT2_UNMAPPED         0x002 // Page fault on Table 2 (invalid PTE) 
#define MMU_ERR_PRIVILEGE_VIOLATION  0x004 // Protected access in user mode 
#define MMU_ERR_WRITE_VIOLATION      0x008 // Write access to a non write page 
#define MMU_ERR_EXEC_VIOLATION       0x010 // Exec access to a non exec page 
#define MMU_ERR_UNDEFINED_XTN        0x020 // Undefined external access address 
#define MMU_ERR_PT1_ILLEGAL_ACCESS   0x040 // Bus Error in Table1 access 
#define MMU_ERR_PT2_ILLEGAL_ACCESS   0x080 // Bus Error in Table2 access 
#define MMU_ERR_CACHE_ILLEGAL_ACCESS 0x100 // Bus Error during the cache access 

///////////////////////////////////////////////////////////////////////////////////
// Page table structure definition
///////////////////////////////////////////////////////////////////////////////////

typedef struct PageTable 
{
    unsigned int pt1[PT1_SIZE / 4];    // PT1 (index is ix1)
    unsigned int pt2[1][PT2_SIZE / 4]; // PT2s (index is 2*ix2)
} page_table_t;

///////////////////////////////////////////////////////////////////////////////////
// functions prototypes
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// This function makes a "virtual" to "physical" address translation,
// using the page table of the calling task.
// The MMU is supposed to be activated.
// It supports both small (4 Kbytes) & big (2 Mbytes) pages.
// The page flags are written in the flags buffer.
// It uses the address extension mechanism for physical addressing.
// Returns the physical address if success, exit if PTE1 or PTE2 unmapped.
///////////////////////////////////////////////////////////////////////////////////
unsigned long long _v2p_translate( unsigned int  vaddr,
                                   unsigned int* flags );

#endif 

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

