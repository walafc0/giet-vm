///////////////////////////////////////////////////////////////////////////////////
// File     : iommu.c
// Date     : 01/09/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The iommu.c and iommu.h files are part ot the GIET-VM nano kernel.
// They contain the functions used to dynamically handle the iommu page table.
///////////////////////////////////////////////////////////////////////////////////

#include <utils.h>
#include <tty_driver.h>
#include <vmem.h>
#include <giet_config.h>
#include <tty_driver.h>

///////////////////////////////////////////////////////////////////////////////////
//    Global variable : IOMMU page table.
///////////////////////////////////////////////////////////////////////////////////

extern page_table_t _iommu_ptab;

///////////////////////////////////////////////////////////////////////////////////
// This function map a PTE2 in IOMMU page table
///////////////////////////////////////////////////////////////////////////////////
void _iommu_add_pte2( unsigned int ix1,
                      unsigned int ix2,
                      unsigned int ppn,
                      unsigned int flags ) 
{
    unsigned int ptba;
    unsigned int * pt_ppn;
    unsigned int * pt_flags;

    // get pointer on iommu page table
    page_table_t* pt = &_iommu_ptab;

    // get ptba and update PT2
    if ((pt->pt1[ix1] & PTE_V) == 0) 
    {
        _printf("\n[GIET ERROR] in iommu_add_pte2() : "
                "IOMMU PT1 entry not mapped / ix1 = %d\n", ix1 );
        _exit();
    }
    else 
    {
        ptba = pt->pt1[ix1] << 12;
        pt_flags = (unsigned int *) (ptba + 8 * ix2);
        pt_ppn = (unsigned int *) (ptba + 8 * ix2 + 4);
        *pt_flags = flags;
        *pt_ppn = ppn;
    }
} // end _iommu_add_pte2()


///////////////////////////////////////////////////////////////////////////////////
// This function unmap a PTE2 in IOMMU page table
///////////////////////////////////////////////////////////////////////////////////
void _iommu_inval_pte2( unsigned int ix1, 
                        unsigned int ix2 ) 
{
    unsigned int ptba;
    unsigned int * pt_flags;

    // get pointer on iommu page table
    page_table_t * pt = &_iommu_ptab;

    // get ptba and inval PTE2
    if ((pt->pt1[ix1] & PTE_V) == 0)
    {
        _printf("\n[GIET ERROR] in iommu_inval_pte2() "
              "IOMMU PT1 entry not mapped / ix1 = %d\n", ix1 );
        _exit();
    }
    else 
    {
        ptba = pt->pt1[ix1] << 12;
        pt_flags = (unsigned int *) (ptba + 8 * ix2);
        *pt_flags = 0;
    }   
} // end _iommu_inval_pte2()

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4


