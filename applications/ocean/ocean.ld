/****************************************************************************
* Definition of the base address for all virtual segments
*****************************************************************************/

seg_code_base      = 0x10000000;
seg_data_base      = 0x20000000;
seg_ldata_base     = 0x20200000;

/***************************************************************************
* Grouping sections into segments for code and data
***************************************************************************/

SECTIONS
{
    . = seg_code_base;
    seg_code : 
    {
        . = ALIGN(0x40);
        *(.text)
        *(.text.*)
        . = ALIGN(0x40);
        *(.rodata)
        *(.rodata.*)
    }
    . = seg_data_base;
    seg_data : 
    {
        . = ALIGN(0x40);
        *(.ctors)
        *(.data)
        *(.lit8)
        *(.lit4)
        *(.sdata)
        *(.bss)
        *(COMMON)
        *(.sbss)
        *(.scommon)
    }
    . = seg_ldata_base;
    seg_ldata :
    {
        . = ALIGN(0x40);
        *(seg_ldata)
    }
}

