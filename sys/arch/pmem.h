/*
 * pmem.h
 *	- physical memory management
 */

#ifndef _SYS_ARCH_PMEM_H_
#define _SYS_ARCH_PMEM_H_

#include <gravity.h>

/*
 * メモリブロック記述子
 */
struct pmem_desc {
	void	*vaddr;	/* 仮想アドレス */
	void	*paddr;	/* 物理アドレス */
	size_t	size;	/* バイト数 */
};

/*
 * エラーコード
 */
#define PMEM_SUCCESS		(0)
#define PMEM_NOSPACE		(1)
#define PMEM_BADDESC		(2)

/*
 * pmem.c
 */
int pmem_alloc_lo(size_t size, struct pmem_desc *desc);	/* paddr < 1GB */
int pmem_alloc_hi(size_t size, struct pmem_desc *desc);	/* paddr >= 1GB */
int pmem_free(struct pmem_desc *desc);
int pmem_lock(struct pmem_desc *desc);
int pmem_unlock(struct pmem_desc *desc);

#endif
