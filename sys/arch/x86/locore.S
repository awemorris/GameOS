#define _ASM_SRC_
#include "asm.h"

.code32
.data

/*
 * address setting
 */
_ADDR_IDT		= ADDR_IDT			/* Interrupt descriptor table */
_ADDR_BOOT_INFO	= ADDR_BOOT_INFO	/* Multiboot Information */
_ADDR_PDT		= ADDR_INIT_PDT		/* Page directory table  */
_ADDR_STACK		= ADDR_INIT_STACK	/* Startup stack */
_ADDR_FREETOP	= ADDR_FREE_TOP		/* Top free address */

/*
 * size setting
 */
_SIZE_GDT		= 8 * (SEG_MAX+1)	/* size of the GDT (5 entries) */
_SIZE_STACK		= 0x1000			/* size of startup stack */

/*
 * page directory table entry attr setting
 */
_PDE_SYS = PTE_PRESENT | PTE_USER | PTE_BIG | PTE_WRITE

/*
 * GDT
 */
gdt_entries:
	.byte 0, 0, 0, 0, 0, 0, 0, 0				/* ignored */
	.byte 0xff, 0xff, 0, 0, 0, 0x98, 0xcf, 0	/* sys code */
	.byte 0xff, 0xff, 0, 0, 0, 0x92, 0xcf, 0	/* sys data */
	.byte 0xff, 0xff, 0, 0, 0, 0xf8, 0xcf, 0	/* user code */
	.byte 0xff, 0xff, 0, 0, 0, 0xf2, 0xcf, 0	/* user data */
	.byte 103,	0,	  0, 0, 0, 0x89, 0, 0		/* TSS */

/*
 * TSS
 */
	.global EXT_C(tss_area)
EXT_C(tss_area):
	.space 104		/* TODO: CPUごとに用意 */

/*
 * message
 */
msg_invalid_bootloader:
	.ascii "Invalid bootloader!\0"
msg_cpuid_not_supported:
	.ascii "Unsupported old CPU! (CPUID not supported)\0"
msg_pse_not_supported:
	.ascii "Unsupported old CPU! (PSE not supported)\0"
msg_fpu_not_implemented:
	.ascii "Unsupported old CPU! (on-chip FPU not implemented)\0"
msg_bad_int:
	.ascii "Bad Interrupt\0"

/*
 * small work area
 */
work_area:
	.space 256


.text

/*
 * システムエントリポイント
 */ 
	.global text_start
text_start:

	/* Multibootヘッダの直後にジャンプする */
	jmp .L_multiboot_header_end

	/* Multibootヘッダ */
	.align 4
  .L_multiboot_header_start:

	.long	0x1badb002					/* magic */
	.long	0x00000003					/* flags */
	.long	-(0x1badb002 + 0x00000003)	/* checksum */

	.align 4
  .L_multiboot_header_end:

/*
 * ブートローダから渡された情報を保存する
 */
save_boot_info:

	/* セグメントレジスタを設定する */
	movw	%ds, %dx
	movw	%dx, %es

	/* ブートローダのシグネチャをチェックする */
	cmpl	$0x2badb002, %eax
	jz		.L_bootloader_magic_ok
	movl	$(msg_invalid_bootloader), %esi
	jmp		on_panic
  .L_bootloader_magic_ok:

	/* ブート情報を保存する(ワークエリアを中継して転送する) */
	movl	%ebx, %esi						/* from ブート情報 */
	movl	$(work_area - SYS_START), %edi	/* to 中継ワークエリア */
	movl	$90, %ecx
	cld
	rep movsb
	movl	$(work_area - SYS_START), %esi	/* from 中継ワークエリア */
	movl	$(_ADDR_BOOT_INFO), %edi		/* to 格納先 */
	movl	$90, %ecx
	rep movsb

/*
 * CPUタイプを調べる(最低限必要な機能が実装されているかチェックする)
 */
identify_cpu:

	/* 物理アドレスでスタックの設定を行う */
	movw	%dx, %ss
	movl	$(_ADDR_STACK + _SIZE_STACK), %esp

	/* EFLAGSをクリアする */
	pushl	$0
	popfl

	/* CPUID命令が利用できることをチェックする */
	pushl	$(EFLAGS_ID)
	popfl
	pushfl
	popl	%eax
	testl	$(EFLAGS_ID), %eax
	jnz		.L_cpuid_ok
	movl	$(msg_cpuid_not_supported - SYS_START), %esi
	jmp		on_panic
  .L_cpuid_ok:

	/* PSE(ページサイズ拡張)が利用できることをチェックする */
	movl	$1, %eax
	cpuid
	testl	$CPUID_FEAT_PSE, %edx
	jnz		.L_pse_ok
	movl	$(msg_pse_not_supported - SYS_START), %esi
	jmp		on_panic
  .L_pse_ok:

/*
 * ページングを有効にする
 */
init_pagin:

	/* PSE(ページサイズ拡張)ビットをセットして4MBページを有効化する */
	movl	$0x00000010, %eax
	movl	%eax, %cr4

	/* ページディレクトリテーブル(PDT)のエントリ(PDE)をゼロクリアする */
	movl	$(_ADDR_PDT), %edi
	movl	$0, %eax
	movl	$1024, %ecx # 1024 entries
	cld
	rep stosl

	/* 仮想アドレス0x80000000から512MBの範囲を物理アドレス0x00000000へマップする */
	/* (ページサイズは4MBで、1段階のページテーブル用いる。) */
	movl	$(_ADDR_PDT + 2048), %edi	/* vaddr from 0x80000000 */
	movl	$(0 + _PDE_SYS), %eax		/* paddr from 0x00000000 */
	movl	$128, %ecx					/* 512MB (128 entries) */
  L_pdtloop:
	movl	%eax, (%edi)				/* set_entry(vaddr <-- paddr) */
	addl	$4, %edi					/* vaddr += 4MB */
	addl	$0x400000, %eax				/* paddr += 4MB */
	loop L_pdtloop

	/* 最下位4MBを1:1マップする(ページング有効化直後に実行を継続するため) */
	movl	$(_ADDR_PDT + 0), %edi 		/* vaddr 0x00000000 */
	movl	$(0 + _PDE_SYS), (%edi)		/* paddr 0x00000000 */

	/* PGビットをセットしてページングを有効化する */
	movl	$(_ADDR_PDT), %eax
	movl	%eax, %cr3
	movl	%cr0, %eax
	orl 	$0x80000000, %eax			/* set pg bit */
	movl	%eax, %cr0

	/* 仮想アドレス0x80000000台にジャンプする */
	ljmp	$(SEG_SYS_CODE), $(.L_sys_area)
  .L_sys_area:

	/* 仮想アドレス0x80000000台でスタックの設定を行う */
	movl	$(_ADDR_STACK + _SIZE_STACK + SYS_START), %esp

/*
 * GDTの初期化を行う (GRUBが用意したGDTを置き換える)
 */
init_gdt:

	/* TSSデスクリプタのベースフィールドを設定する */
	movl	$(gdt_entries + SEG_TSS), %edi
	movl	$(tss_area), %eax
	movb	%al, 2(%edi);	shrl	$8, %eax;
	movb	%al, 3(%edi);	shrl	$8, %eax;
	movb	%al, 4(%edi);	shrl	$8, %eax;
	movb	%al, 7(%edi)

	/* GDTRにGDTをロードする */
	movl	%esp, %ebx
	subl	$8, %ebx
	movw	$(_SIZE_GDT - 1),	0(%ebx)
	movl	$(gdt_entries),		2(%ebx)
	lgdt	(%ebx)

	/* 作成したセグメント上にジャンプする */
	ljmp	$(SEG_SYS_CODE), $(.L_new_seg)
  .L_new_seg:

	/* セグメントを設定する */
	movl	$(SEG_SYS_DATA), %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
	movl	$(SEG_TSS), %eax
	ltr 	%ax

/*
 * 一時的なIDTを作成する
 */
create_idt:
	/* エントリを作成する */
	movl	$(_ADDR_IDT + SYS_START), %edi
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
	movl	$(_ADDR_IDT + SYS_START), %edi
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
	movl	$(_ADDR_IDT + SYS_START), 2(%ebx)
	lidt	(%ebx)

/*
 * Cのコードにジャンプする
 */
jump_to_extern_c:

	/* jump to cmain() */
	jmp EXT_C(cmain)


/*
 * メッセージを表示して停止する
 * (%esiにNUL終端文字列のアドレスを指定する)
 */
on_panic:
	/* 文字列を表示する */
	movl	$0xB8000, %edi
	movb	$0x0f, %al		/* text attr */
	cld
  .L_on_panic_msg_loop:
	cmpb	$0, (%esi)
	jz		.L_on_panic_hlt_loop
	movsb	/* text char */
	stosb	/* text attr */
	jmp		.L_on_panic_msg_loop

	/* 停止する */
  .L_on_panic_hlt_loop:
	hlt
	jmp		.L_on_panic_hlt_loop

/*
 * 割り込みハンドラ
 */
on_bad_int:
	movl	$(msg_bad_int), %esi
	jmp		on_panic


/*
 * extern "C" linkage procedures
 */

/* void _asm_cli() */
	.global EXT_C(asm_cli)
EXT_C(asm_cli):
	cli
	ret

/* void _asm_sti() */
	.global EXT_C(asm_sti)
EXT_C(asm_sti):
	sti
	ret

/* void _asm_outb(uint16 port, uint8 data) */
	.global EXT_C(asm_outb)
EXT_C(asm_outb):
	pushl	%ebp
	movl	%esp, %ebp

	movl	 8(%ebp), %edx	/* port */
	movl	12(%ebp), %eax	/* data */

	outb	%al, %dx

	popl	%ebp
	ret

/* void _asm_outw(uint16 port, uint8 data) */
	.global EXT_C(asm_outw)
EXT_C(asm_outw):
	pushl	%ebp
	movl	%esp, %ebp

	movl	 8(%ebp), %edx	/* port */
	movl	12(%ebp), %eax	/* data */

	outw	%ax, %dx

	popl	%ebp
	ret

/* uint8 _asm_inb(uint16 port) */
	.global EXT_C(asm_inb)
EXT_C(asm_inb):
	pushl	%ebp
	movl	%esp, %ebp

	movl	8(%ebp), %edx	/* port */

	xorl	%eax, %eax
	inb		%dx, %al

	popl	%ebp
	ret

/* uint8 _asm_inw(uint16 port) */
	.global EXT_C(asm_inw)
EXT_C(asm_inw):
	pushl	%ebp
	movl	%esp, %ebp

	movl	8(%ebp), %edx	/* port */

	xorl	%eax, %eax
	inw		%dx, %ax

	popl	%ebp
	ret

/* void _asm_fxsave(void *save_area) */
	.global EXT_C(asm_fxsave)
EXT_C(asm_fxsave):
	movl	4(%esp), %eax
	fxsave	(%eax)
	ret

/* void _asm_fxrstor(void *save_area) */
	.global EXT_C(asm_fxrstor)
EXT_C(asm_fxrstor):
	movl	4(%esp), %eax
	fxrstor	(%eax)
	ret

/* void _asm_fnsave(void *save_area) */
	.global EXT_C(asm_fnsave)
EXT_C(asm_fnsave):
	movl	4(%esp), %eax
	fnsave	(%eax)
	ret

/* void _asm_frstor(void *save_area) */
	.global EXT_C(asm_frstor)
EXT_C(asm_frstor):
	movl	4(%esp), %eax
	frstor	(%eax)
	ret

/* uint32 _asm_get_eflags() */
	.global EXT_C(asm_get_eflags)
EXT_C(asm_get_eflags):
	pushf
	popl	%eax
	ret

/* uint32 _asm_get_eip() */
	.global EXT_C(asm_get_eip)
EXT_C(asm_get_eip):
	call .L_eip
  .L_eip:
	popl	%eax
	ret

/* uint32 _asm_get_esp() */
	.global EXT_C(asm_get_esp)
EXT_C(asm_get_esp):
	pushl	%esp
	popl	%eax
	ret

/* uint32 _asm_get_cr3() */
	.global EXT_C(asm_get_cr3)
EXT_C(asm_get_cr3):
	movl	%cr3, %eax
	ret

/* uint32 _asm_set_cr3() */
	.global EXT_C(asm_set_cr3)
EXT_C(asm_set_cr3):
	movl	4(%esp), %eax
	movl	%eax, %cr3
	ret

/* void _asm_hlt() */
	.global EXT_C(asm_hlt)
EXT_C(asm_hlt):
	hlt
	ret

/* void _asm_lidt(void *idt_desc) */
	.global EXT_C(asm_lidt)
EXT_C(asm_lidt):
	movl	4(%esp), %eax
	lidt	(%eax)
	ret

/* void _asm_load_pdt(uint32 addr) */
	.global EXT_C(asm_load_cr3)
EXT_C(asm_load_cr3):
	movl	4(%esp), %eax
	movl	%eax, %cr3
	ret

/* void _asm_flash_tlb() */
	.global EXT_C(asm_flash_tlb)
EXT_C(asm_flash_tlb):
	movl	%cr3, %eax
	movl	%eax, %cr3
	ret

