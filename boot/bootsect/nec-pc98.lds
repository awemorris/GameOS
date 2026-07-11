/*
 * Link the IPL as raw code starting at 0x0.
 * The actual address will be:
 *   0x1fc00: IDE HDD
 *   0x1fe00: FDD
 */

OUTPUT_FORMAT("elf32-i386")

ENTRY(_start)

SECTIONS {
    . = 0;
    .text : {
	    *(.text*) *(.rodata*) *(.data*)
    }
}
