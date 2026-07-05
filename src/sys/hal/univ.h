/*
 * univ.h
 *  - user address space (universe) management
 */

#ifndef _SYS_ARCH_UNIV_H_
#define _SYS_ARCH_UNIV_H_

#include <sys/types.h>


/*
 * universe handle (equals to a pointer to _univ_info)
 */
typedef void *univ_t;

/*
 * システム空間を表すuniv_t値 (実際には存在しないユニバースである)
 */
#define UNIV_SYS	(NULL)

/*
 * ページ属性
 */
#define PAGE_NONE		(0)
#define PAGE_READ		(1)
#define PAGE_WRITE		(2)
#define PAGE_EXEC		(4)
#define PAGE_NOCACHE	(8)

/*
 * univ.c
 */
univ_t univ_create();
void univ_destroy(univ_t u);
int univ_check_handle(univ_t u);
void univ_switch(univ_t u);
void univ_set_entry(
	univ_t	u,		/* アドレス空間 */
	void	*vaddr,	/* 仮想アドレス(ユーザ空間, ページ境界) */
	void	*paddr,	/* 物理アドレス(ページ境界) */
	size_t	size,	/* バイト数(ページサイズ境界) */
	int		attr	/* 属性 */
);

#endif
