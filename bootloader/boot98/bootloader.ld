/*
 * Link the second stage as if it starts at offset 0 within its own
 * segment (LOAD_SEG = 0x0800, so linear 0x8000). The IPL far-jumps
 * to 0800:0000, so internal 16-bit offsets are relative to 0.
 */

OUTPUT_FORMAT("elf32-i386")

ENTRY(_start)

SECTIONS {
	. = 0x0000; .text : {
        *(.text*) *(.rodata*) *(.data*) *(.bss*)
	}
}
