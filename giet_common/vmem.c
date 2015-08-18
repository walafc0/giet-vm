///////////////////////////////////////////////////////////////////////////////////
// File     : vmem.c
// Date     : 01/07/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <tty0.h>
#include <utils.h>
#include <vmem.h>
#include <ctx_handler.h>
#include <giet_config.h>

///////////////////////////////////////////////////////
unsigned long long _v2p_translate( unsigned int  vaddr,
                                   unsigned int* flags )
{
    unsigned long long ptba;
    unsigned long long pte2_paddr;

    unsigned int pte2_msb;
    unsigned int pte2_lsb;
    unsigned int flags_value;
    unsigned int ppn_value;

    unsigned int save_sr;
  
    // decode the vaddr fields
    unsigned int offset = vaddr & 0xFFF;
    unsigned int ix1    = (vaddr >> 21) & 0x7FF;
    unsigned int ix2    = (vaddr >> 12) & 0x1FF;

    // get page table vbase address
    page_table_t* pt = (page_table_t*)_get_context_slot(CTX_PTAB_ID);

    // get PTE1
    unsigned int pte1 = pt->pt1[ix1];

    // check PTE1 mapping
    if ( (pte1 & PTE_V) == 0 )
    {
        _printf("\n[VMEM ERROR] _v2p_translate() : pte1 unmapped\n"
                "  vaddr = %x / ptab = %x / pte1_vaddr = %x / pte1_value = %x\n",
                vaddr , (unsigned int)pt, &(pt->pt1[ix1]) , pte1 );
        _exit();
    }

    // test big/small page
    if ( (pte1 & PTE_T) == 0 )  // big page
    {
        *flags = pte1 & 0xFFC00000;
        offset = offset | (ix2<<12);
        return (((unsigned long long)(pte1 & 0x7FFFF)) << 21) | offset;
    }
    else                        // small page
    {

        // get physical addresses of pte2
        ptba       = ((unsigned long long)(pte1 & 0x0FFFFFFF)) << 12;
        pte2_paddr = ptba + 8*ix2;

        // split physical address in two 32 bits words
        pte2_lsb   = (unsigned int) pte2_paddr;
        pte2_msb   = (unsigned int) (pte2_paddr >> 32);

        // disable interrupts and save status register
        _it_disable( &save_sr );

        // get ppn_value and flags_value, using a physical read
        // after temporary DTLB desactivation
        asm volatile (
                "mfc2    $2,     $1          \n"     /* $2 <= MMU_MODE       */
                "andi    $3,     $2,    0xb  \n"
                "mtc2    $3,     $1          \n"     /* DTLB off             */

                "move    $4,     %3          \n"     /* $4 <= pte_lsb        */
                "mtc2    %2,     $24         \n"     /* PADDR_EXT <= pte_msb */
                "lw      %0,     0($4)       \n"     /* read flags           */ 
                "lw      %1,     4($4)       \n"     /* read ppn             */
                "mtc2    $0,     $24         \n"     /* PADDR_EXT <= 0       */

                "mtc2    $2,     $1          \n"     /* restore MMU_MODE     */
                : "=r" (flags_value), "=r" (ppn_value)
                : "r"  (pte2_msb)   , "r"  (pte2_lsb)
                : "$2", "$3", "$4" );

        // restore saved status register
        _it_restore( &save_sr );

        // check PTE2 mapping
        if ( (flags_value & PTE_V) == 0 )
        {
            _printf("\n[VMEM ERROR] _v2p_translate() : pte2 unmapped\n"
                    "  vaddr = %x / ptab = %x / pte1_value = %x\n"
                    "  pte2_paddr = %l / ppn = %x / flags = %x\n",
                    vaddr , pt , pte1 , pte2_paddr ,  ppn_value , flags_value );
            _exit();
        }

        *flags = flags_value & 0xFFC00000;
        return (((unsigned long long)(ppn_value & 0x0FFFFFFF)) << 12) | offset;
    }
} // end _v2p_translate()



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4


