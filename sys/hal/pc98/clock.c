#include "sys/hal/i386/clock.h"
#include "sys/hal/i386/irq.h"
#include "sys/hal/i386/asm.h"
#include "sys/hal/i386/pic.h"
#include "sys/sched.h"

//
// macro: HAL_PCAT for IBM PC/AT compatible
// macro: HAL_PC98 for NEC PC-9801 compatible
//

/* CPUのtickカウント */
static clock_t cpu_tick_count;

/* forward declaration */
static void init_8254();

/*
 * クロック管理部を初期化する
 */
void clock_init()
{
	/* 時刻をゼロリセットする */
	cpu_tick_count = 0;

	/* PIT(8254)を初期化する */
	init_8254();

	/* タイマIRQのマスクを解除する(この瞬間からタイマ割り込みが発生する) */
	pic_set_irq_mask(IRQ_TIMER, 0);
}

/*
 * CPUのローカル時刻を取得する
 */
clock_t	clock_get_tick_count()
{
	return cpu_tick_count;
}

/*
 * タイマ割り込みハンドラ
 */
void clock_handler()
{
	/* 時刻をインクリメントする */
	cpu_tick_count++;
}

/* PIT(8254)を初期化する */
static void init_8254()
{
//	uint16 interval = _SCHED_INTERVAL_USEC / 1000;
	uint16 interval = 0;

	/*
	 * インターバルタイマの設定を行う
	 */

	/* set rate-generator mode */
	asm_outb(0x43, 0x34);
		/* ------------------------------------
		 *  0x34 = (0011 0100)b
		 * ------------------------------------
		 *	 00 [7-6]: ch0
		 *   11 [5-4]: 16bit load (LSB first)
		 *   010[3-1]: rate generator mode
		 *   0  [0]  : binary count
		 * ------------------------------------ */

	/* rate LSB */
	asm_outb(0x40, interval&0xff);

	/* rate MSB */
	asm_outb(0x40, interval>>8);
}
