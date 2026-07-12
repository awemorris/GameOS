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

	cons_init();
	pmem_init();
	irq_init();
	int_init();
	univ_init();
	task_init();
	sched_init();
	clock_init();

	t = task_create(UNIV_SYS, testmain, NULL, NULL);
	sched_link(t, SCHED_LIST_ACTIVE, SCHED_PRIOR_LOW, 0);

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
