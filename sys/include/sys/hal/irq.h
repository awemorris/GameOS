/*
 * irq.h
 *  - 
 */

#ifndef _SYS_ARCH_IRQ_H_
#define _SYS_ARCH_IRQ_H_

/*
 * ローカルIRQ割り込みロック
 */
typedef int irqlock_t;

#define ENTER_IRQLOCK(v)				\
	do {						\
		v = irq_acquire_lock();			\
	} while(0);

#define LEAVE_IRQLOCK(v)				\
	do {						\
		irq_unacquire_lock(v);			\
	} while(0)					

/*
 * irq.c
 */
irqlock_t irq_acquire_lock();			/* ローカルIRQ割り込みを禁止する */
void irq_unacquire_lock(irqlock_t lock);	/* 割り込み特権レベルを復元する */
void irq_enter_isr(int irq_num);		/* IRQ発生を待ってIRQ処理を開始する */
void irq_leave_isr(int irq_num);		/* IRQ処理を終了する */


#endif
