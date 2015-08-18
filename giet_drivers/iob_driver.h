///////////////////////////////////////////////////////////////////////////////////
// File     : iob_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The iob_driver.c and iob_driver.h files are part ot the GIET-VM kernel.
// This driver supports the TSAR vci_io_bridge, that is a bridge to access
// The external peripherals, implementing an IO_MMU.
// The SEG_IOB_BASE virtual addresses must be defined in hard_config.h file.
// The physical base address is (cluster_io << 32) | SEG_IOB_BASE.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_IOB_DRIVER_H_
#define _GIET_IOB_DRIVER_H_

///////////////////////////////////////////////////////////////////////////////////
//                registers offsets and iommu error codes
///////////////////////////////////////////////////////////////////////////////////

enum IOB_registers 
{
    IOB_IOMMU_PTPR       = 0,      // R/W : Page Table Pointer Register
    IOB_IOMMU_ACTIVE     = 1,      // R/W : IOMMU activated if not 0
    IOB_IOMMU_BVAR       = 2,      // R   : Bad Virtual Address (unmapped) 
    IOB_IOMMU_ETR        = 3,      // R   : Error Type 
    IOB_IOMMU_BAD_ID     = 4,      // R   : Faulty Peripheral Index (SRCID)
    IOB_INVAL_PTE        = 5,      // W   : Invalidate a PTE (virtual address)
    IOB_WTI_ENABLE       = 6,      // R/W : Enable WTIs (both IOMMU and peripherals)
    IOB_WTI_ADDR_LO      = 7,      // W/R : 32 LSB bits for IOMMU WTI
    IOB_WTI_ADDR_HI      = 8,      // W/R : 32 MSB bits for IOMMU WTI
};

enum mmu_error_type_e 
{
    MMU_NONE                      = 0x0000, // None
    MMU_WRITE_ACCES_VIOLATION     = 0x0008, // Write access to a non writable page 
    MMU_WRITE_PT1_ILLEGAL_ACCESS  = 0x0040, // Write Bus Error accessing Table 1       
    MMU_READ_PT1_UNMAPPED 	      = 0x1001, // Read  Page fault on Page Table 1  	
    MMU_READ_PT2_UNMAPPED 	      = 0x1002, // Read  Page fault on Page Table 2  
    MMU_READ_PT1_ILLEGAL_ACCESS   = 0x1040, // Read  Bus Error in Table1 access      
    MMU_READ_PT2_ILLEGAL_ACCESS   = 0x1080, // Read  Bus Error in Table2 access 	
    MMU_READ_DATA_ILLEGAL_ACCESS  = 0x1100, // Read  Bus Error in cache access 
};


///////////////////////////////////////////////////////////////////////////////////
//                       access functions
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// This function invalidates a TLB entry identified by a virtual address.
///////////////////////////////////////////////////////////////////////////////
extern void _iob_inval_tlb_entry( unsigned int cluster_xy,
                                  unsigned int vaddr );

///////////////////////////////////////////////////////////////////////////////
// This function sets a new value in IOB_IOMMU_PTPR register.
///////////////////////////////////////////////////////////////////////////////
extern void _iob_set_iommu_ptpr(  unsigned int cluster_xy,
                                  unsigned int value );



#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

