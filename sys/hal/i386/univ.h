/*
 * univ.h
 *  - 多重仮想空間サポート
 */

#ifndef _SYS_ARCH_X86_UNIV_H_
#define _SYS_ARCH_X86_UNIV_H_

#include "sys/hal/univ.h"		/* interface definition */

/*
 * ユニバース構造体
 */
struct univ_info {
	uint32 pdt[1024];		/* ページディレクトリ */
	int univ_id;			/* univ-ID */
	struct ptbl *ptbl_head;		/* ページテーブル */
	struct univ_info *next;		/* 管理用リンクリスト */
};

/*
 * ページテーブルリストノード
 */
struct ptbl {
	uint32		v_addr;		/* このノードが表す先頭仮想アドレス */
	uint32		*pte;		/* ページテーブル(4KB境界) */
	struct ptbl	*next;		/* 次のノード */
};

/*
 * univ.c
 */
void univ_init();

#endif
