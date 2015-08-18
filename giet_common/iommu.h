///////////////////////////////////////////////////////////////////////////////////
// File     : iommu.h
// Date     : 01/09/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#ifndef _IOMMU_H_
#define _IOMMU_H_

#include <giet_config.h>
#include <mapping_info.h>

////////////////////////////////////////////////////////////////////////////////////
// functions prototypes
////////////////////////////////////////////////////////////////////////////////////

void _iommu_add_pte2( unsigned int ix1, 
                      unsigned int ix2, 
                      unsigned int ppn, 
                      unsigned int flags );

void _iommu_inval_pte2( unsigned int ix1, 
                        unsigned int ix2 );

#endif 

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

