/*
 * Let the current task be a "Task ISR".
 *
 * Task ISR is a task-based, deferred IRQ handler that the IRQ
 * interrupt handler schedules. Task ISR is deferred and has latency,
 * but it can use kernel blocking features such as I/O.
 *
 * When this API is called, the current task will be removed from the
 * normal scheduling list and the execution is yielded to aother task.
 * The task will be scheduled again when the specified IRQ happened,
 * then it resumes by the kernel scheduler.
 *
 * [Usage]
 *   while (1) {
 *       hal_irq_enter_task_isr(IRQ_SOME_DEVICE);
 *       // Do IRQ stuff.
 *   }
 */
void hal_irq_enter_task_isr(int irq_num, uint32_t flags);

/* Leave the Task-ISR. (This will unmask the IRQ.) */
void hal_irq_leave_task_isr(int irq_num);

/*
 * Wait for a specified IRQ and start ISR.
 */
void
hal_irq_enter_task_isr(
	int irq_num)
{
	task_t t;
	bool need_restore;

	need_restore = irq_disable();

	/* Check if a serice task is already registered. */
	assert(irq_service_task[irq_num] == NULL);

	/*
	 * Get the running task and register it to the interrupt
	 * service task for irq_num.
	 */
	irq_service_task[irq_num] = task_get_current();

	/*
	 * Unlink the task from the schedule list.
	 * (The task will sleep after a preemption or a yield.)
	 */
	sched_link(t, SCHED_LIST_UNLINKED, 0, 0);

	/* Unmask irq_num. */
	pic_set_irq_mask(irq_num, 0);

	if (need_restore)
		irq_enable();

	/*
	 * Yield the task's execution right.
	 * This task will sleep until it is rescheduled by IRQ.
	 */
	sched_yield();
}

task_isr_handler(void *p)
{
	/* Set the IRQ mask for irq_num. */
	hal_irq_set_mask(irq_num);

	/* Send EOI. */
	hal_irq_send_eoi(irq_num);

	/* Get the service task. */
	t = irq_service_task[irq_num];
	assert(t != NULL);

	/* Unregister the service task. */
	irq_service_task[irq_num] = NULL;

	/* Push back the service task to the normal schedule list to resume it. */
	sched_link(t, SCHED_LIST_ACTIVE, SCHED_PRIOR_HIGH, 0);

	/* Set the rescheduling flag. */
	int_set_resched_flag();
}
