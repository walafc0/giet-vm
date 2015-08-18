/****************************************************************************
* Definition of the base address for all virtual segments
*****************************************************************************/

seg_code_base      = 0x10000000;
seg_data_0_base    = 0x20000000;
seg_data_1_base    = 0x30000000;

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
    . = seg_data_0_base;
    seg_data_0 : 
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
        *(.data_in)
    }
    . = seg_data_1_base;
    seg_data_1 : 
    {
        *(.data_out)
    }
}

