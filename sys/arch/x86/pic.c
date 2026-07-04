#include "sys/arch/x86/pic.h"
#include "sys/arch/x86/int.h"
#include "sys/arch/x86/asm.h"


/*
 * 割り込みコントローラを初期化する
 */
void pic_init()
{
	/* 8259A(マスタ)を初期化する */
	asm_outb(0x20, 0x11);			/* 初期化開始, エッジトリガ/カスケード接続 */
	asm_outb(0x21, INT_IRQ_BASE);	/* INT E0h-EFh */
	asm_outb(0x21, 0x04);			/* IR2をスレーブに接続する */
	asm_outb(0x21, 0x01);			/* 80x86モード */

	/* 8259A(スレーブ)を初期化する */
	asm_outb(0xa0, 0x11);			/* 初期化開始, エッジトリガ/カスケード接続 */
	asm_outb(0xa1, INT_IRQ_BASE+8);/* INT E8h-EFh */
	asm_outb(0xa1, 0x02);			/* マスタのIR2に接続する */
	asm_outb(0xa1, 0x01);			/* 80x86モード */

	/* すべてのIRQをマスクする */
	asm_outb(0x21, 0xff);
	asm_outb(0xa1, 0xff);
}

/*
 * IRQマスクを設定する
 */
void pic_set_irq_mask(
	int	irq_num,	/* IRQ番号 */
	int	mask)		/* 0: 許可する, 1: マスク(禁止)する */
{
	if(irq_num < 8) {
		uint8 imr = asm_inb(0x21);
		if(mask) imr |=  (1 << irq_num);
		else     imr &= ~(1 << irq_num);
		asm_outb(0x21, imr);
	} else {
		uint8 imr = asm_inb(0xa1);
		if(mask) imr |=  (1 << (irq_num&7));
		else     imr &= ~(1 << (irq_num&7));
		asm_outb(0xa1, imr);
	}
}

/*
 * サービス中のIRQ番号を取得する
 */
int pic_get_irq_in_service()
{
	uint8	in_service;
	int		ret;

	/* 割り込みコントローラのISRレジスタ(サービス中IRQ番号)を読む */
	asm_outb(0x20, 0x0B);
	in_service = asm_inb(0x20);

	/* IRQ番号を求める(立っているビットを探す) */
	ret = -1;
	while(in_service != 0) {
		ret++;
		in_service >>= 1;
	}
	return ret;
}

/*
 * EOIを送信する
 */
void pic_send_eoi(int irq_num)
{
	/* EOIを送信する */
	if(irq_num <= 7) {
		/* マスターの場合 */
		asm_outb(0x20, 0x20);	/* マスターにEOIを送信する */
	} else {
		/* スレーブの場合 */
		asm_outb(0xA0, 0x20);		/* スレーブにEOIを送信する */
		asm_outb(0xA0, 0x0B);		/* スレーブのISRを読む */
		if(asm_inb(0xA0) == 0)		/* 処理が残っていなければ */
			asm_outb(0x20, 0x20);		/* マスターにもEOIを送信する */
	}
}
