#include <sys/types.h>
#include <sys/kcrt/kcrt.h>
#include "asm.h"
#include "pmem.h"
#include "task.h"

static void idle_task();

extern void cons_init(void);
extern void mem_init(void);
extern void irq_init(void);
extern void int_init(void);
extern void univ_init(void);
extern void task_init(void);
extern void sched_init(void);
extern void clock_init(void);
extern void testmain(void);

void cmain()
{
	task_t t;

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
	int index = 0;
#if defined(HAL_BOARD_PCAT)
	uint16 *vram = (uint16 *)(0x800b8000 + 158);
#endif
#if defined(HAL_BOARD_PC98)
	uint16 *vram = (uint16 *)(0x800a0000 + 158);
#endif

	for(;;) {
#if defined(HAL_BOARD_PCAT)
		*vram = animation[index] | 0x2100;
#endif
#if defined(HAL_BOARD_PC98)
		*vram = animation[index];
#endif
		index = (index + 1) % 4;
		asm_hlt();
	}
}
