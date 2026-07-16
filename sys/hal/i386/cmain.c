#include <sys/kern/hal.h>

#include "i386.h"

/*
 * HAL C code entry point.
 */
void hal_main()
{
	/* First, setup the console for error messages. */
	bsp_cons_init();

	/* Setup paging. */
	i386_page_init();

	/* Setup interrupt handlers. */
	i386_int_init();

	/* Setup tasking. */
	i386_task_init();

	/* Setup IRQ. */
	bsp_irq_init();

	/* Setup interval timer and realtime clock. */
	bsp_timer_init();

	/* Jump to kernel. */
	kernel_entry();
}
