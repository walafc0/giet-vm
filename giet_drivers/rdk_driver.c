///////////////////////////////////////////////////////////////////////////////
// File      : rdk_driver.c
// Date      : 13/02/2014
// Author    : alain greiner
// Maintainer: cesar fuguet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <rdk_driver.h>
#include <utils.h>
#include <tty0.h>

#if !defined(SEG_RDK_BASE) 
# error: You must define SEG_RDK_BASE in the hard_config.h file
#endif

/////////////////////////////////////////////////////
unsigned int _rdk_access( unsigned int       use_irq,    // not used
                          unsigned int       to_mem,
                          unsigned int       lba, 
                          unsigned long long buf_vaddr,  // actually vaddr
                          unsigned int       count) 
{
#if USE_IOC_RDK

#if GIET_DEBUG_IOC_DRIVER
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH) - 1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
_printf("\n[RDK DEBUG] P[%d,%d,%d] enters _rdk_access at cycle %d\n"
        "  use_irq = %d / to_mem = %d / lba = %x / paddr = %x / count = %d\n",
        x , y , p , _get_proctime() , use_irq , to_mem , lba , buf_vaddr, count );
#endif

    char* rdk = (char*)SEG_RDK_BASE + (512*lba);
    char* buf = (char*)buf_paddr;

    if ( to_mem ) memcpy( buf, rdk, count*512 );
    else          memcpy( rdk, buf, count*512 );

    return 0;

#else

    _printf("[RDK ERROR] _rdk_access() but USE_IOC_RDK not set\n");
    return 1;

#endif
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

