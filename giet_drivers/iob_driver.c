///////////////////////////////////////////////////////////////////////////////////
// File     : iob_driver.c
// Date     : 23/05/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <iob_driver.h>
#include <utils.h>

#if !defined(SEG_IOB_BASE) 
# error: You must define SEG_IOB_BASE in the hard_config.h file
#endif

#if !defined(GIET_USE_IOMMU) 
# error: You must define GIET_USE_IOMMU in the giet_config.h file
#endif

#if !defined( USE_IOB )
# error: You must define USE_IOB in the hard_config.h file
#endif


///////////////////////////////////////////////////////////////////////////////
// This low level function returns the value contained in register "index"
// in the IOB component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////
unsigned int _iob_get_register( unsigned int cluster_xy,   // cluster index
                                unsigned int index )       // register index
{
    unsigned long long paddr = (unsigned long long)SEG_IOB_BASE + 
                               ((unsigned long long)cluster_xy << 32) +
                               ((unsigned long long)index << 2);

    return _physical_read( paddr );
}
///////////////////////////////////////////////////////////////////////////////
// This low level function sets a new value in register "index"
// in the IOB component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////
void _iob_set_register( unsigned int cluster_xy,       // cluster index
                        unsigned int index,            // register index
                        unsigned int value )           // value to be written
{
    unsigned long long paddr = (unsigned long long)SEG_IOB_BASE + 
                               ((unsigned long long)cluster_xy << 32) +
                               ((unsigned long long)index << 2);

    _physical_write( paddr, value );
}




///////////////////////////////////////////////////
void _iob_inval_tlb_entry( unsigned int cluster_xy,
                           unsigned int vaddr )
{
    _iob_set_register( cluster_xy,
                       IOB_INVAL_PTE,
                       vaddr );
}

//////////////////////////////////////////////////
void _iob_set_iommu_ptpr( unsigned int cluster_xy,
                          unsigned int value )
{
    _iob_set_register( cluster_xy,
                       IOB_IOMMU_PTPR,
                       value );
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

