#ifndef _SYS_ARCH_X86_IRQ_H_
#define _SYS_ARCH_X86_IRQ_H_

#include "sys/hal/irq.h"	/* interface definition */
#include "sys/hal/task.h"	/* task_t */

/*
 * IRQ番号
 */
#define	IRQ_MAX			(15)
#define IRQ_TIMER		(0)
#define IRQ_KEYBOARD	(1)

/*
 * IRQサービス登録情報
 */
struct irq_service_info {
	task_t	ist;	/* 割り込みサービスタスク */
};

/*
 * irq.c
 */
void irq_init(void);
void irq_handler(int irq_num);	/* called from int.c */
void irq_unmask_i8254(void);
int  irq_get_in_service(void);

#endif
