
/****************************************************************************/
/* Definition of the base addresses for all vsegs used by the GIET_VM       */
/****************************************************************************/

INCLUDE giet_vsegs.ld

/****************************************************************************/
/* Definition of the entry point for the BOOT code.                         */ 
/* The address of the boot_init function is stored in the .elf header,      */
/* and is used by the preloader to jump into the boot code.                 */
/****************************************************************************/

ENTRY(boot_entry)

/****************************************************************************/
/* Grouping sections into virtual segment for boot code and data             */
/****************************************************************************/
SECTIONS
{
    . = boot_code_vbase;
    seg_boot_code : 
    {
        *(.text)
        *(.rodata)
        *(.rodata.*)
    }

    . = boot_data_vbase;
    seg_boot_data :
    {
        *(.kdata)
    }
}

