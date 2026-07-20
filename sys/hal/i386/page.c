/*
 * i386 Memory Management
 */

#include "multiboot.h"

#define PAGEMAP_GET(n)		(pagemap_tbl[(n)>>5] & (1<<((n)&31)))
#define PAGEMAP_SET(n)		(pagemap_tbl[(n)>>5] |= (1<<((n)&31)))
#define PAGEMAP_RESET(n)	(pagemap_tbl[(n)>>5] &= ~(1<<((n)&31)))

/*
 * Page Table
 */
struct page_table_info {
	uintptr_t vaddr;
	uint32_t *pte;
	struct page_table_info *next;
};

/*
 * Space
 */
struct space_info {
	uint32_t pdt[1024];		/* Page Directory Table */
	int space_id;			/* Space ID */
	struct ptbl *ptbl_head;		/* Page Table */
	struct space_info *next;
};

/*
 * Number of Physical Pages
 */
static uint32_t phys_pages;

/*
 * Page Usage Table
 */
static uint32_t *pagemap_tbl;

/*
 * Space List
 */
struct hal_space_info *space_list;

/*
 * Current Selected Space
 */
hal_space_t *cur_space;

/*
 * Top Unused ID
 */
int free_id_top;

/*
 * Forward declaration
 */
static void init_memory_map(void);

/*
 * Initialize the page module.
 */
void
i386_mem_init(void)
{
	cur_space = HAL_SPACE_SYS;
	free_id_top = 1;

	/* Initialize the memory map. */
	init_memory_map();
}

static void
init_memory_map(void)
{
	struct multiboot_info *mbi;
	uint32	total, avail_top, i;

	/*
	 * Get the memory size from the multiboot info.
	 * Multiboot info is passed by a boot loader such as GRUB.
	 * (Our testing boot loader too passes it.)
	 */
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
 *  - カーネルアドレス空間から直接アクセス可能な下位領域(<1GB)のみ使用する
 *  - pmem_lock()によるロックを行わずにアクセスできる
 */
int pmem_alloc(size_t size, struct pmem_desc *desc)
{
	uint32	need_pages;	/* 割り当てるページ数 */
	uint32	start_index;	/* 割り当て先頭ページ */
	uint32	page_end;	/* 先頭ページとして利用可能な最後のページ */
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

	/* Succeeded. */
	return PMEM_SUCCESS;
}

/*
 * アドレス空間を作成する
 */
univ_t univ_create()
{
	struct univ_info *ui;
	int i;

	/* 構造体のメモリを確保して初期化する */
	ui = (struct univ_info *)malloc(sizeof(struct univ_info));
	ui->univ_id = free_id_top++;
	ui->ptbl_head = NULL;
	ui->next = NULL;

	/* PDTを初期化する */
	for(i=0; i<1024; i++)
		ui->pdt[i] = 0;
	for(i=0; i<128; i++)
		ui->pdt[512+i] = (i*0x400000)|(PTE_PRESENT|PTE_USER|PTE_BIG|PTE_WRITE);

	/* univ_tにキャストして返す */
	return (univ_t) ui;
}

/*
 * アドレス空間を切り替える
 */
void univ_switch(univ_t u)
{
	struct univ_info *ui;

	/* 変更がない場合 */
	if(u == cur_univ)
		return;

	/* univ_infoにキャストする */
	ui = (struct univ_info *)u;

	/* PDTを切り替える */
	asm_load_cr3((uint32)ui->pdt - SYS_START);

	puts("\n[univ changed!]");
}

/*
 * univ値が正しいかチェックする
 */
int univ_is_valid(univ_t u)
{
	struct univ_info *ui;

	ui = (struct univ_info *)u;

	/* カーネル空間の場合 */
	if(ui == UNIV_SYS)
		return 1;	/* 正しい値 */

	/* 不正な値*/
	return 0;
}
