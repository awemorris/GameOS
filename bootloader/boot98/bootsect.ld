/*
 * Link the IPL as raw code starting at 0x7C00, matching where the
 * PC-98 BIOS loads it. Output will be flat-binary via objcopy.
 */


OUTPUT_FORMAT("elf32-i386")

ENTRY(_start)

SECTIONS {
    . = 0;
    .text : {
	    *(.text*) *(.rodata*) *(.data*)
    }
}
