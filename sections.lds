ENTRY(text_start)

KERNEL_VMA_BASE = 0x80100000;
KERNEL_LMA_BASE = 0x00100000;

SECTIONS
{
    . = KERNEL_VMA_BASE;

    /*
     * .text
     *
     * File-backed.
     *
     * VMA  : 0x80100000
     * LMA  : 0x00100000
     * File : usually 0x001000
     */
    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VMA_BASE + KERNEL_LMA_BASE)
    {
        . = ALIGN(4);
        text_start = .;
        _text_start = .;

        *(.text)
        *(.text.*)

        . = ALIGN(4);
        _text_end = .;
    }

    _text_size = _text_end - _text_start;

    /*
     * .data
     *
     * File-backed.
     *
     * Keep all file-backed writable/read-only data here.
     */
    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VMA_BASE + KERNEL_LMA_BASE)
    {
        . = ALIGN(4);
        _data_start = .;

        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)

        *(.rodata)
        *(.rodata.*)
        *(.srodata)
        *(.srodata.*)

        . = ALIGN(4);
        _data_end = .;
    }

    _data_size = _data_end - _data_start;

    /*
     * .meta
     *
     * File-backed.
     *
     * Put this BEFORE .bss so that file offsets continue to track
     * VMA/LMA layout for all file-backed sections.
     */
    .meta ALIGN(4096) : AT(ADDR(.meta) - KERNEL_VMA_BASE + KERNEL_LMA_BASE)
    {
        . = ALIGN(4);
        _meta_start = .;

        *(.note.gnu.build-id)

        . = ALIGN(4);
        _meta_end = .;
    }

    _meta_size = _meta_end - _meta_start;

    /*
     * .bss
     *
     * Not file-backed.
     *
     * Put it LAST so that its NOBITS nature does not disturb the
     * file-offset <-> physical-address relation of later sections.
     */
    .bss ALIGN(4096) : AT(ADDR(.bss) - KERNEL_VMA_BASE + KERNEL_LMA_BASE)
    {
        . = ALIGN(4);
        _bss_start = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4);
        _bss_end = .;
    }

    _bss_size = _bss_end - _bss_start;

    _kernel_end = .;
}
