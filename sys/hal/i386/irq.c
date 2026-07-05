#include <sys/hal/clock.h>
#include <sys/kcrt/kcrt.h>
#include <sys/kern/sched.h>
#include "irq.h"
#include "pic.h"
#include "asm.h"
#include "clock.h"
#include "int.h"


/* IRQサービス情報 */
static struct irq_service_info irq_service[IRQ_MAX+1];


/*
 * IRQ管理部を初期化する
 */
void irq_init()
{
	int i;

	/* IRQの情報を初期化する */
	for(i=0; i<=IRQ_MAX; i++)
		irq_service[i].ist = NULL;

	/* 割り込みコントローラを初期化する(すべてのIRQはマスクされる) */
	pic_init();
}

/*
 * ローカルIRQ割り込みを禁止する
 */
irqlock_t irq_acquire_lock()
{
	int status = asm_get_eflags() & EFLAGS_IF;
	asm_cli();
	return status;
}

/*
 * 割り込み特権レベルを復元する
 */
void irq_unacquire_lock(irqlock_t lock)
{
	if(lock != 0)
		asm_sti();
}

/*
 * IRQ発生を待ってIRQ処理を開始する
 */
void irq_enter_isr(int irq_num)
{
	task_t		t;
	irqlock_t	irqlock;

	/* 割り込み禁止区間 */
	ENTER_IRQLOCK(irqlock)
	{
		/* すでにサービスタスクが登録されていないかチェックする */
		CRT_ASSERT(irq_service[irq_num].ist == NULL);

		/* 実行中のタスクを取得してISRタスクとして登録する */
		t = task_get_current();
		irq_service[irq_num].ist = t;

		/* スケジュールリストからunlinkする */
		/* (以降、プリエンプトされるかyieldするとスリープ状態になる) */
		sched_link(t, SCHED_LIST_UNLINKED, 0, 0);

		/* 当該IRQのマスクを解除する */
		pic_set_irq_mask(irq_num, 0);
	}
	LEAVE_IRQLOCK(irqlock);

	/* タスク実行権を譲る */
	/* (IRQ発生によって再スケジュールされるまでは実行されなくなる) */
	sched_yield();

	/*
	 * IRQの発生によって再びスケジュールリストにリンクされ、実行が再開される
	 */
}

/*
 * 割り込みサービスルーチンの実行終了時にコールする
 */
void irq_leave_isr(int irq_num)
{
}

/*
 * IRQ割り込みハンドラ
 * ※割り込み禁止区間内で実行される
 */
void irq_handler(int irq_num)
{
	task_t t;

	/*
	 * タイマ割り込みをハンドルする
	 */
	if(irq_num == IRQ_TIMER) {
		/* クロックハンドラをコールする */
		clock_handler();

		/* スケジューラの割り込みハンドラを実行する */
		sched_clock_handler();

		/* EOIを送信してIRQ処理を完了する */
		pic_send_eoi(irq_num);
		return;
	}

	/*
	 * その他の割り込みの場合
	 */

	/* 当該IRQのマスクをセットする */
	pic_set_irq_mask(irq_num, 1);

	/* EOIを送信する */
	pic_send_eoi(irq_num);

	/* 登録されているサービスタスクを取得する */
	t = irq_service[irq_num].ist;
	CRT_ASSERT(t != NULL);

	/* サービスタスクの登録を抹消する */
	irq_service[irq_num].ist = NULL;

	/* タスクをスケジュールリストにリンクして再開させる */
	sched_link(t, SCHED_LIST_ACTIVE, SCHED_PRIOR_HIGH, 0);

	/* 再スケジュールフラグをセットする */
	int_set_resched_flag();
}
