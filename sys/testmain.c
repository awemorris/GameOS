#include <sys/types.h>
#include "sys/hal/univ.h"
#include "sys/hal/task.h"
#include "sys/hal/cons.h"
#include "sys/hal/clock.h"
#include "sys/kcrt/kcrt.h"
#include "sys/kern/sched.h"

static const char msg_version[]	= "gravity 0.0.1 booted.\n";

static univ_t	u1;
static task_t	t1, t2, t3;

static void *make_stack(uint32 push_param);
static void test_task1(int param);
static void test_task2(int param);
static void test_task3(int param);

void testmain()
{
	crt_printf("Comtemporary Time Sharing System Monitor 0.0.1\n");

	/* Make a task for testing. (task1) */
	t1 = task_create(UNIV_SYS, test_task1, (void *) 1, NULL);
	sched_link(t1, SCHED_LIST_ACTIVE, SCHED_PRIOR_LOW, 0);

	/* Let this thread become console. */
	crt_printf("% ");
	while(1) {
		char c;
		c = cons_getc();
		crt_putchar(c);
	}
}

void test_task1(int param)
{
	crt_printf("\nstart1(%d)\n", param);

	/* さらにタスクを作成する */
	t2 = task_create(UNIV_SYS, test_task2, (void *) 2, NULL);
	sched_link(t2, SCHED_LIST_ACTIVE, SCHED_PRIOR_LOW, 0);

	for(;;) {
		sched_link(t1, SCHED_LIST_TIMEWAIT, 0, 10);
		sched_yield();

		crt_printf(" task(1): local cpu clock count=%d\n", clock_get_tick_count());
	}
}

void test_task2(int param)
{
	crt_printf("\nstart2(%d)\n", param);

	/* ユニバースを作成する */
	u1 = univ_create();

	for(;;) {
		sched_link(task_get_current(), SCHED_LIST_TIMEWAIT, SCHED_PRIOR_LOW, 20);
		sched_yield();

		crt_printf("   task(2): local cpu clock count=%d\n",  clock_get_tick_count());
	}
}

void test_task3(int param)
{
	int i, j;

	crt_printf("\nstart3(%d)\n", param);

	for(;;) {
		for(i=0;i<65536;i++)
			for(j=0; j<256; j++)
				;

		crt_putchar('K');
	}
}
