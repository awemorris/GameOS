/*
 * system configuration
 */

#ifndef _SYS_ARCH_X86_ASM_H_
#define _SYS_ARCH_X86_ASM_H_

/*
 * システムデバイスの設定
 */

/* ローカルクロックの周波数(100Hz) */
#define CLOCK_HZ			(100)

/*
 * メモリ空間の設定
 */

/* 対応する最大物理メモリサイズ(MB) */
#define PHYSICAL_MEGS		(4096)	/* 4GB */

/* ページサイズ(Area-U, Area-S1) */
#define PAGE_SIZE			(4096)

/* 固定システム空間(Area-S0)の開始アドレス */
#define SYS_START			(0x80000000)

/* 可変システム空間(Area-S1)の開始アドレス */
#define SYS_V_START			(0xc0000000)

/* 固定的に利用する低位アドレス */
#define ADDR_NULL		(0x00000000)	/* 未使用 */
#define ADDR_IDT		(0x00001000)	/* 割り込みデスクリプタテーブル */
#define ADDR_BOOT_INFO		(0x00002000)	/* ブートローダから渡された情報 */
#define ADDR_INIT_STACK		(0x00003000)	/* スタートアップスタック */
#define ADDR_TEMP_PPAGE_MAP	(0x00004000)	/* pmem.cの初期化用作業領域 */
#define ADDR_INIT_PDT		(0x00005000)	/* 初期ページディレクトリテーブル */
#define ADDR_FREE_TOP		(0x00006000)	/* 低位アドレスのfree top  */
#define ADDR_INIT_PT		(0x00020000)	/* Initial Page Table */

/*
 * セグメントの設定
 */

/* セレクタ値 */
#define SEG_INVALID		(0x0000)	/* 無効セレクタ値 */
#define SEG_SYS_CODE		(0x0008)	/* システムコード */
#define SEG_SYS_DATA		(0x0010)	/* システムデータ/スタック */
#define SEG_USER_CODE		(0x0018)	/* ユーザコード */
#define SEG_USER_DATA		(0x0020)	/* ユーザデータ/スタック */
#define SEG_TSS			(0x0028)	/* TSS (今のところ唯一のTSS) */
#define SEG_MAX			SEG_TSS

/* RPL(要求者特権レベル, セレクタ値の下位2ビット) */
#define SEG_RPL_0		(0)
#define SEG_RPL_1		(1)
#define SEG_RPL_2		(2)
#define SEG_RPL_3		(3)

/*
 * 割り込み番号の設定
 */
#define INT_DIVBYZERO		(0x00)
#define INT_GPE				(0x0d)
#define INT_PAGEFAULT		(0x0e)
#define INT_SYSCALL			(0xc2)
#define INT_IRQ_BASE		(0xe0)
#define INT_UNDEF			(0xffffffff)

/*
 * フラグレジスタのビット
 */

/* EFLAGSレジスタのフラグ */
#define EFLAGS_CF		(0x000001)	/* Carry*/
#define EFLAGS_PF		(0x000004)	/* Parity */
#define EFLAGS_AF		(0x000010)	/* Auxiliary (BCD carry/borrow) */
#define EFLAGS_ZF		(0x000040)	/* Zero */
#define EFLAGS_SF		(0x000080)	/* Sign */
#define EFLAGS_TF		(0x000100)	/* Trap (Single-Step Debug) */
#define EFLAGS_IF		(0x000200)	/* Interrupt Enable */
#define EFLAGS_DF		(0x000400)	/* Direction (String) */
#define EFLAGS_OF		(0x000800)	/* 0verflow */
#define EFLAGS_IOPL_0	(0x000000)	/* IOPL-0 */
#define EFLAGS_IOPL_1	(0x001000)	/* IOPL-1 */
#define EFLAGS_IOPL_2	(0x002000)	/* IOPL-2 */
#define EFLAGS_IOPL_3	(0x003000)	/* IOPL-3 */
#define EFLAGS_NT		(0x004000)	/* Nested Task */
#define EFLAGS_RF		(0x010000)	/* Resume (Debug Resume) */
#define EFLAGS_VM		(0x020000)	/* VM86 */
#define EFLAGS_AC		(0x040000)	/* Alignment Check */
#define EFLAGS_VIF		(0x080000)	/* Virtual Interrupt Flag (VME)*/
#define EFLAGS_VIP		(0x100000)	/* Virtual Interrupt Pending (VME) */
#define EFLAGS_ID		(0x200000)	/* Identification (CPUID Support) */

/* PTEのフラグ */
#define PTE_PRESENT		(0x0001)	/* Present */
#define PTE_WRITE		(0x0002)	/* User Write */
#define PTE_USER		(0x0004)	/* User/Supervisor */
#define PTE_WRITEBACK	(0x0008)	/* Enable Cache-Writeback */
#define PTE_NOCACHE		(0x0010)	/* Disable Cache */
#define PTE_ACCESS		(0x0020)	/* Accessed */
#define PTE_DIRTY		(0x0040)	/* Written */
#define PTE_BIG			(0x0080)	/* 4MB Page Size (PDE) */
#define PTE_GLOBAL		(0x0100)	/* No TLB flush */

/* CPUIDのフィーチャフラグ (EDX on EAX=1) */
#define	CPUID_FEAT_FPU		(0x00000001)	/* On-Chip FPU */
#define CPUID_FEAT_PSE		(0x00000008)	/* Page Size Extension */


/*
 * extern "C" linkage procedures
 */

/* extern "C" リンケージのシンボル名 */
#define EXT_C(s)	s

#ifndef _ASM_SRC_
#include <sys/types.h>
	void	asm_cli();
	void	asm_sti();
	void	asm_outb(uint16 port, uint8 data);
	void	asm_outw(uint16 port, uint16 data);
	uint8	asm_inb(uint16 port);
	uint16	asm_inw(uint16 port);
	void	asm_fxsave(void *save_area);
	void	asm_fxrstor(void *save_area);
	void	asm_fnsave(void *save_area);
	void	asm_frstor(void *save_area);
	uint32	asm_get_eflags();
	uint32	asm_get_eip();
	uint32	asm_get_esp();
	uint32	asm_get_cr3();
	uint32	asm_set_cr3();
	void	asm_hlt();
	void	asm_lidt(void *idt_desc);
	void	asm_load_cr3(uint32 addr);
	void	asm_flash_tlb();
#endif	// ASM

#endif
