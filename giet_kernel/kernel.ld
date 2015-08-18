
/******************************************************************************/
/* Definition of the base addresses for all vsegs used by the GIET_VM         */
/******************************************************************************/

INCLUDE giet_vsegs.ld

/******************************************************************************/
/* Grouping sections into virtual segments for system code and data           */
/******************************************************************************/

SECTIONS
{
    . = kernel_code_vbase;
    seg_kernel_code : 
    {
        *(.giet)
        *(.text)
    }

    . = kernel_data_vbase;
    seg_kernel_data : 
    {
        *(.kdata)
    }

    . = kernel_init_vbase;
    seg_kernel_init : 
    {
        *(.kinit)
    }
}

