#include "defs.h"

.code32
.data

/*
 * PDT/PT
 */
NUM_HIGH_PDE   = 64		/* 64 * 4MB = 256MB */
NUM_PTE_TOTAL  = 64 * 1024      /* 256MB / 4KB */
PDE_SYS = PTE_PRESENT | PTE_USER | PTE_WRITE
PTE_SYS = PTE_PRESENT | PTE_USER | PTE_WRITE

.text

/*
 * Kernel entry point.
 */ 
.global text_start
text_start:

	/* Near-jump to right after the multiboot header. */
	jmp .L_multiboot_header_end

	/*
	 * Multiboot header to be recognized by grub bootloader.
	 * Note that our own boot loaders don't check the multiboot header.
	 */
	.align 4
  .L_multiboot_header_start:

	.long	0x1badb002			/* magic */
	.long	0x00000003			/* flags */
	.long	-(0x1badb002 + 0x00000003)	/* checksum */

	.align 4
  .L_multiboot_header_end:

/*
 * Save multiboot info.
 */
save_boot_info:

	/* Check for multiboot signature. */
	cmpl	$0x2badb002, %eax
	jz	.L_bootloader_magic_ok
	movl	$(msg_invalid_bootloader), %esi
	jmp	on_panic
  .L_bootloader_magic_ok:

	/* Copy multiboot info to a work area. */
	movl	%ebx, %esi
	movl	$(work_area - SYS_START), %edi
	movl	$90, %ecx
	cld
	rep movsb

	/* Copy mutiboot info to the destination. */
	movl	$(work_area - SYS_START), %esi
	movl	$(ADDR_BOOT_INFO), %edi
	movl	$90, %ecx
	rep movsb

/*
 * Setup GDT.
 */
init_gdt:

	movl $(0xa0000), %edi
	movb $'3', (%edi)

	/* GDTRにGDTをロードする */
	movl	$(gdtr - SYS_START), %ebx
	lgdt	(%ebx)

	/* 作成したセグメント上にジャンプする */
	ljmp	$(SEG_SYS_CODE), $(.L_new_seg - 0x80000000)
    .L_new_seg:

	/* セグメントを設定する */
	movl	$(SEG_SYS_DATA), %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

/*
 * Enable paging.
 */
init_paging:

	/*
	 * Clear the page directory table.
	 *
	 * PDT has 1024 entries.
	 * Each entry is 4 bytes.
	*/
	movl    $(ADDR_INIT_PDT), %edi
	xorl    %eax, %eax
	movl    $1024, %ecx
	cld
	rep    stosl

	/*
	* Build page tables for 256MB.
	*
	* We place 64 page tables immediately after PDT:
	*
	*   ADDR_INIT_PT + 0x00000 : PT0
	*   ADDR_INIT_PT + 0x01000 : PT1
	*   ADDR_INIT_PT + 0x02000 : PT2
	*   ...
	*   ADDR_PT + 0x3f000 : PT64
	*
	* Each PT maps 4MB using 1024 entries.
	*
	* Total:
	*
	*   64 * 1024 PTEs = 65536 PTEs
	*   65536 * 4KB   = 256MB
	*/
	movl    $(ADDR_INIT_PT), %edi
	movl    $(0x00000000 + PTE_SYS), %eax
	movl    $(NUM_PTE_TOTAL), %ecx
	cld

    .L_fill_pte:
	/*
	* PTE format without PSE:
	*
	*   bits 31..12 : physical page base address
	*   bits 11..0  : flags
	*
	* eax starts as:
	*
	*   0x00000000 | flags
	*
	* and increases by 0x1000 for each 4KB page.
	*/
	stosl
	addl    $0x1000, %eax
	loop    .L_fill_pte

	/*
	* Fill PDE[512] - PDE[576].
	*
	* 0x80000000 / 4MB = 512
	*
	* So PDE[512] corresponds to virtual 0x80000000.
	*
	* Each PDE points to one 4KB page table.
	*/
	movl    $(ADDR_INIT_PDT + 512 * 4), %edi
	movl    $(ADDR_INIT_PT + PDE_SYS), %eax
	movl    $(NUM_HIGH_PDE), %ecx

  .L_fill_high_pde:
	/*
	* PDE points to the physical address of a page table.
	*
	* PDE[512] -> PT0  maps 0x80000000 - 0x803fffff
	* PDE[513] -> PT1  maps 0x80400000 - 0x807fffff
	* PDE[514] -> PT2  maps 0x80800000 - 0x80bfffff
	* ...
	*/
	movl    %eax, (%edi)
	addl    $4, %edi
	addl    $0x1000, %eax
	loop    .L_fill_high_pde

	/*
	* Identity-map the lowest 4MB.
	*
	* This is needed because immediately after CR0.PG is set, the CPU is
	* still executing the next instruction using the current linear address.
	*
	* PDE[0] -> PT0
	*
	* Therefore:
	*
	*   0x00000000 - 0x003fffff
	*
	* is mapped to:
	*
	*   0x00000000 - 0x003fffff
	*/
	movl    $(ADDR_INIT_PT + PDE_SYS), %eax
	movl    %eax, (ADDR_INIT_PDT + 0 * 4)

	/*
	* Load CR3 with the physical address of PDT.
	*/
	movl    $(ADDR_INIT_PDT), %eax
	movl    %eax, %cr3

	/*
	* Enable paging by setting CR0.PG.
	*/
	movl    %cr0, %eax
	orl    $0x80000000, %eax
	movl    %eax, %cr0

	/*
	* Jump to the higher-half address.
	*
	* After this point, code runs at virtual address 0x80000000+.
	*/
	ljmp    $(SEG_SYS_CODE), $(.L_sys_area)
    .L_sys_area:

	/*
	 * Set startup stack in higher-half virtual address space.
	 */
	movl    $(ADDR_INIT_STACK + SIZE_INIT_STACK + SYS_START), %esp

/*
 * Setup TSS.
 */
init_tss:

	/* TSSデスクリプタのベースフィールドを設定する */
	movl	$(gdt + SEG_TSS), %edi
	movl	$(tss_area), %eax
	movb	%al, 2(%edi)
	shrl	$8, %eax
	movb	%al, 3(%edi)
	shrl	$8, %eax
	movb	%al, 4(%edi)
	shrl	$8, %eax;
	movb	%al, 7(%edi)

	movl	$(SEG_TSS), %eax
	ltr 	%ax

/*
 * 一時的なIDTを作成する
 */
create_idt:

	/* エントリを作成する */
	movl	$(ADDR_IDT + SYS_START), %edi
	movl	$(on_bad_int), %eax
	movl	%eax, %ebx;	shrl	$16, %ebx
	movw	%ax,				0(%edi)
	movw	$(SEG_SYS_CODE),	2(%edi)
	movb	$0,					4(%edi)
	movb	$(0x8e),			5(%edi)
	movw	%bx,				6(%edi)
	movl	0(%edi), %eax
	movl	4(%edi), %ebx

	/* エントリを256個分コピーする */
	movl	$(ADDR_IDT + SYS_START), %edi
	movl	$256, %ecx
  .L_idt:
	movl	%eax, 0(%edi)
	movl	%ebx, 4(%edi)
	addl	$8, %edi
	loop .L_idt

	/* IDTRにロードする */
	movl	%esp, %ebx
	subl	$8, %ebx
	movw	$(256*8-1), (%ebx)
	movl	$(ADDR_IDT + SYS_START), 2(%ebx)
	lidt	(%ebx)

/*
 * Cのコードにジャンプする
 */
jump_to_extern_c:

	/* jump to cmain() */
	jmp cmain

/*
 * Print message and halt.
 *  ESI = message
 */
on_panic:
	call print_message
  .L_on_panic_hlt_loop:
	hlt
	jmp .L_on_panic_hlt_loop

/*
 * Print message.
 *  ESI = message
 */
print_message:
#if defined(HAL_BOARD_PCAT)
	movl $0xb8000, %edi
	movb $0x0f, %al
	cld
    .L_msg_copy_loop:
	cmpb $0, (%esi)
	jz .L_msg_copy_end
	movsb
	stosb
    .L_msg_copy_end:
	ret
#endif
#if defined(HAL_BOARD_PC98)
	movl $0xa0000, %edi
	xor %al, %al
	cld
    .L_msg_copy_loop:
	cmpb $0, (%esi)
	jz .L_msg_copy_end
	movsb
	stosb
    .L_msg_copy_end:
	ret
#endif

/*
 * Initial interrupt handler.
 */
on_bad_int:
	movl	$(msg_bad_int), %esi
	jmp		on_panic

/*
 * extern "C" linkage procedures
 */

/* void asm_cli() */
	.global EXT_C(asm_cli)
EXT_C(asm_cli):
	cli
	ret

/* void asm_sti() */
	.global EXT_C(asm_sti)
EXT_C(asm_sti):
	sti
	ret

/* void asm_outb(uint16 port, uint8 data) */
	.global EXT_C(asm_outb)
EXT_C(asm_outb):
	pushl	%ebp
	movl	%esp, %ebp

	movl	 8(%ebp), %edx	/* port */
	movl	12(%ebp), %eax	/* data */

	outb	%al, %dx

	popl	%ebp
	ret

/* void asm_outw(uint16 port, uint8 data) */
	.global EXT_C(asm_outw)
EXT_C(asm_outw):
	pushl	%ebp
	movl	%esp, %ebp

	movl	 8(%ebp), %edx	/* port */
	movl	12(%ebp), %eax	/* data */

	outw	%ax, %dx

	popl	%ebp
	ret

/* uint8 asm_inb(uint16 port) */
	.global EXT_C(asm_inb)
EXT_C(asm_inb):
	pushl	%ebp
	movl	%esp, %ebp

	movl	8(%ebp), %edx	/* port */

	xorl	%eax, %eax
	inb		%dx, %al

	popl	%ebp
	ret

/* uint8 asm_inw(uint16 port) */
	.global EXT_C(asm_inw)
EXT_C(asm_inw):
	pushl	%ebp
	movl	%esp, %ebp

	movl	8(%ebp), %edx	/* port */

	xorl	%eax, %eax
	inw		%dx, %ax

	popl	%ebp
	ret

/* void asm_fxsave(void *save_area) */
	.global EXT_C(asm_fxsave)
EXT_C(asm_fxsave):
	movl	4(%esp), %eax
	fxsave	(%eax)
	ret

/* void asm_fxrstor(void *save_area) */
	.global EXT_C(asm_fxrstor)
EXT_C(asm_fxrstor):
	movl	4(%esp), %eax
	fxrstor	(%eax)
	ret

/* void asm_fnsave(void *save_area) */
	.global EXT_C(asm_fnsave)
EXT_C(asm_fnsave):
	movl	4(%esp), %eax
	fnsave	(%eax)
	ret

/* void asm_frstor(void *save_area) */
	.global EXT_C(asm_frstor)
EXT_C(asm_frstor):
	movl	4(%esp), %eax
	frstor	(%eax)
	ret

/* uint32 asm_get_eflags() */
	.global EXT_C(asm_get_eflags)
EXT_C(asm_get_eflags):
	pushf
	popl	%eax
	ret

/* uint32 asm_get_eip() */
	.global EXT_C(asm_get_eip)
EXT_C(asm_get_eip):
	call .L_eip
  .L_eip:
	popl	%eax
	ret

/* uint32 asm_get_esp() */
	.global EXT_C(asm_get_esp)
EXT_C(asm_get_esp):
	pushl	%esp
	popl	%eax
	ret

/* uint32 asm_get_cr3() */
	.global EXT_C(asm_get_cr3)
EXT_C(asm_get_cr3):
	movl	%cr3, %eax
	ret

/* uint32 asm_set_cr3() */
	.global EXT_C(asm_set_cr3)
EXT_C(asm_set_cr3):
	movl	4(%esp), %eax
	movl	%eax, %cr3
	ret

/* void asm_hlt() */
	.global EXT_C(asm_hlt)
EXT_C(asm_hlt):
	hlt
	ret

/* void asm_lidt(void *idt_desc) */
	.global EXT_C(asm_lidt)
EXT_C(asm_lidt):
	movl	4(%esp), %eax
	lidt	(%eax)
	ret

/* void asm_load_pdt(uint32 addr) */
	.global EXT_C(asm_load_cr3)
EXT_C(asm_load_cr3):
	movl	4(%esp), %eax
	movl	%eax, %cr3
	ret

/* void asm_flash_tlb() */
	.global EXT_C(asm_flash_tlb)
EXT_C(asm_flash_tlb):
	movl	%cr3, %eax
	movl	%eax, %cr3
	ret

/*
 * Messages
 */
msg_boot:			.ascii "Kernel booted.\0"
msg_invalid_bootloader:		.ascii "Invalid bootloader!\0"
msg_cpuid_not_supported:	.ascii "Unsupported old CPU! (CPUID not supported)\0"
msg_pse_not_supported:		.ascii "Unsupported old CPU! (PSE not supported)\0"
msg_fpu_not_implemented:	.ascii "Unsupported old CPU! (on-chip FPU not implemented)\0"
msg_bad_int:			.ascii "Bad Interrupt\0"

/*
 * GDT (Direct Mapped)
 */
gdt:
	.byte 0, 0, 0, 0, 0, 0, 0, 0			/* 0x0000: ignored */
	.byte 0xff, 0xff, 0, 0, 0, 0x9a, 0xcf, 0	/* 0x0008: sys code */
	.byte 0xff, 0xff, 0, 0, 0, 0x92, 0xcf, 0	/* 0x0010: sys data */
	.byte 0xff, 0xff, 0, 0, 0, 0xf8, 0xcf, 0	/* 0x0018: user code */
	.byte 0xff, 0xff, 0, 0, 0, 0xf2, 0xcf, 0	/* 0x0020: user data */
	.byte 103,  0,    0, 0, 0, 0x89, 0,    0	/* 0x0028: TSS */
gdt_end:

/*
 * GDTR
 */
gdtr:
	.word gdt_end - gdt - 1
	.long gdt - 0x80000000

/*
 * TSS
 */
	.global EXT_C(tss_area)
EXT_C(tss_area):
	.space 104		/* TODO: CPUごとに用意 */

/*
 * small work area
 */
work_area:
	.space 256
