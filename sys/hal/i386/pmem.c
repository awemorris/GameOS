/*
 * Physical Memory Management
 */

#include <sys/hal/irq.h>
#include <sys/kcrt/kcrt.h>
#include "pmem.h"
#include "asm.h"
#include "multiboot.h"

#define PAGEMAP_GET(n)		(pagemap_tbl[(n)>>5] & (1<<((n)&31)))
#define PAGEMAP_SET(n)		(pagemap_tbl[(n)>>5] |= (1<<((n)&31)))
#define PAGEMAP_RESET(n)	(pagemap_tbl[(n)>>5] &= ~(1<<((n)&31)))

/*
 * Number of Physical Pages
 */
static uint32 phys_pages;

/*
 * Page Usage Table
 */
static uint32 *pagemap_tbl;

/*
 * Forward declaration
 */
static void init_pagemap_tbl(void);

/*
 * Initialize pmem module.
 */
void pmem_init(void)
{
	init_pagemap_tbl();
}

/* 物理メモリのマッピングを検出する */
static void init_pagemap_tbl(void)
{
	struct multiboot_info *mbi;
	uint32	total, avail_top, i;

	/* ブート情報のメモリ項目を利用できることを確認する */
	mbi = (struct multiboot_info *) (SYS_START + ADDR_BOOT_INFO);
	if(!(mbi->flags & MBINFO_FLAG_MEMORY))
		fatal("Can't detect memory size");

	/* 物理メモリサイズを取得する */
	total = ((uint32)mbi->mem_upper + 1024) * 1024;	/* 上位メモリ(kb)+下位1024kb */
	phys_pages = total / PAGE_SIZE;
	printf("Memory: %d kb detected.\n", total / 1024);
	if(total < 0x400000)
		fatal("Too few physical memory");

	/* TODO: 利用可能な先頭アドレスを取得する */
	avail_top = 0x200000;

	/* ページ使用状況テーブルを作成する */
	pagemap_tbl = (uint32 *) avail_top;
	avail_top += (phys_pages + 31) / 32;
	memset(pagemap_tbl, 0, (phys_pages+31)/32);

	/* 利用できないページにマークを付ける */
	/* (TODO: ブート情報のメモリマップを利用, 下位メモリも利用可能に) */
	avail_top = (avail_top + PAGE_SIZE - 1) / PAGE_SIZE;
	for (i=0; i<avail_top; i++)
		PAGEMAP_SET(i);
}

/*
 * 連続した物理メモリをページ単位で割り当てる
 *	o カーネルアドレス空間から直接アクセス可能な下位領域(<1GB)のみ使用する
 *	o pmem_lock()によるロックを行わずにアクセスできる
 */
int pmem_alloc_lo(size_t size, struct pmem_desc *desc)
{
	uint32	need_pages;		/* 割り当てるページ数 */
	uint32	start_index;	/* 割り当て先頭ページ */
	uint32	page_end;		/* 先頭ページとして利用可能な最後のページ */
	uint32	i;

	/* 割り当てるページ数を求める */
	need_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	/* 先頭ページとして利用可能な最後のページを求める */
	page_end = phys_pages - need_pages;

	/* 割り当てできるまで空きページを探す */
	start_index = 0;
	for(;;) {
		/* 最初の空きページを探す */
		for(; start_index<=page_end; start_index++) {
			if(PAGEMAP_GET(start_index) == 0) {
				break;	/* 空きページを見つけた */
			}
		}
		if(start_index > page_end)
			break;	/* 空きページがみつからなかった */

		/* 必要な連続空きページを確保できるか調べる */
		for(i=0; i<need_pages; i++) {
			if(PAGEMAP_GET(start_index + i) != 0)
				break;	/* 使用済みページが見つかった */
		}
		if(i == need_pages)
			break;	/* 空きページが見つかった */

		/* 空きページがみつからなかった場合 */
		start_index += i + 1;	/* 使用済みページの次ページから再試行する */
	}

	/* 空き領域が見つからなかった場合 */
	if(i > page_end)
		return PMEM_NOSPACE;

	/* 見つかった空き領域を使用済みとする */
	for(i=0; i<need_pages; i++)
		PAGEMAP_SET(start_index + i);

	/* デスクリプタに情報を設定する */
	desc->paddr = (void *) (start_index << 12);
	desc->vaddr = (void *) ((start_index << 12) | SYS_START);
	desc->size	= need_pages << 12;

	/* 成功 */
	return PMEM_SUCCESS;
}

/*
 * 連続した物理メモリをページ単位で割り当てる
 *	o カーネルアドレス空間から直接アクセスできない下位領域(>=1GB)も使用する
 *	o pmem_lock()によるロックを行わないとアクセスできない
 */
int pmem_alloc_hi(size_t size, struct pmem_desc *desc)
{
	/* 未実装 */
	return pmem_alloc_lo(size, desc);
}

/*
 * ページ単位で割り当てた物理メモリを解放する
 */
int pmem_free(struct pmem_desc *desc)
{
	uint32 start_page, end_page, i;

	/* ブロックのページ範囲を取得する */
	start_page = (uint32)desc->paddr >> 12;
	end_page   = start_page + (desc->size >> 12);

	/* ページをチェックする */
	for(i=start_page; i<=end_page; i++) {
		/* 未使用のページが検出された場合 */
		if(PAGEMAP_GET(i) == 0)
			return PMEM_BADDESC;	/* エラー */
	}

	/* ページを解放する */
	for(i=start_page; i<=end_page; i++)
		PAGEMAP_RESET(i);	/* ページを未使用にする */

	/* 成功 */
	return PMEM_SUCCESS;
}

/*
 * ページブロックを仮想アドレス空間にマップする
 */
int pmem_lock(struct pmem_desc *desc)
{
	/* 未実装 */
	return PMEM_SUCCESS;
}

int pmem_unlock(struct pmem_desc *desc)
{
	/* 未実装 */
	return PMEM_SUCCESS;
}
