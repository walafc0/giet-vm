/****************************************************************************
* Definition of the base address for code and data vsegs
*****************************************************************************/

seg_code_base        = 0x10000000;
seg_data_base        = 0x20000000;

/***************************************************************************
* Grouping sections into segments for code and data
***************************************************************************/

SECTIONS
{
    . = seg_code_base;
    seg_code : 
    {
        *(.text)
    }

    . = seg_data_base;
    seg_data : 
    {
        *(.ctors)
        *(.rodata)
        *(.rodata.*)
        *(.data)
        *(.lit8)
        *(.lit4)
        *(.sdata)
        *(.bss)
        *(COMMON)
        *(.sbss)
        *(.scommon)
    }
}

