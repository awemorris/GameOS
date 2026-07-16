void kernel_entry(void)
{
	task_t t;

	sched_init();

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
