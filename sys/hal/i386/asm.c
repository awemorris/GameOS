#include <sys/types.h>
#include <sys/kcrt/kcrt.h>
#include "asm.h"
#include "pmem.h"
#include "task.h"

static void idle_task();

void cmain()
{
	task_t t;
	extern void cons_init();
	extern void mem_init();
	extern void irq_init();
	extern void int_init();
	extern void univ_init();
	extern void task_init();
	extern void sched_init();
	extern void clock_init();
	extern void testmain();

	/* 依存関係に基づいた順でサブモジュールを初期化する */
	cons_init();	/* 簡易コンソール		*/
	//	smp_init(); 	/* プロセッサ(UP/SMP)	*/
	pmem_init(); 	/* 物理ページ管理		*/
	irq_init(); 	/* IRQ					*/
	int_init(); 	/* 割り込み 			*/
	univ_init();	/* アドレス空間 		*/
	task_init();	/* タスク				*/
	//	io_init();		/* I/Oアクセス			*/
	sched_init();	/* スケジューラ 		*/
	clock_init();	/* インターバルクロック */

	/* テスト用メインのタスクを作成する起動する */
	t = task_create(UNIV_SYS, testmain, NULL, NULL);
	sched_link(t, SCHED_LIST_ACTIVE, SCHED_PRIOR_LOW, 0);

	/* 実行中のCPU用のアイドルタスクとなる */
	idle_task();
}

static void idle_task()
{
	static char animation[4] = {'/', '-', '\\', '|'};
	int 		index = 0;
	uint16		*vram = (uint16 *)(0x800B8000 + 158);

	for(;;) {
		*vram = animation[index] | (0x2100);
		index = (index + 1) % 4;
		asm_hlt();
	}
}
