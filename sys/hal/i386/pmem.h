/*
 * pmem.h
 *	- physical memory management
 */

#ifndef _SYS_ARCH_X86_PMEM_H_
#define _SYS_ARCH_X86_PMEM_H_

#include <sys/hal/pmem.h>		/* interface definition */

/*
 * pmem.c
 */
void pmem_init(void);

#endif

/*
 * 各4KBページの利用状況
 *	0x00000000			  : 未使用ページ
 *	0x00000001-0x00100000 : 確保済み先頭ページ (確保したページ数)
 *	0x00100001-0xffefffff : 予約済み
 *	0xfff00000-0xfffffffe : 確保済み途中ページ
 *							(先頭ページとの相対ページ数, 2の補数)
 *	0xffffffff			  : 利用不能ページ ((uint32) -1)
 */
