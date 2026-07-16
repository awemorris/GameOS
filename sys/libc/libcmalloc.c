#include <sys/types.h>

/*
 * todo: pmem_palloc()はページ単位の管理なので、lib.cで細かい管理を行う。
 */

void *crt_malloc(size_t size)
{
	int		err;
	uint32	alloc_size;
	struct pmem_desc desc;

	/* 先頭にデスクリプタを付加するために1ページ分追加する */
	alloc_size = size + 4096;

	/* ページを確保する */
	err = pmem_alloc_lo(alloc_size, &desc);
	if(err != PMEM_SUCCESS)
		CRT_FATAL("pmem_palloc failed!");

	/* 先頭にデスクリプタを付加する */
	crt_memcpy((void *) desc.vaddr, &desc, sizeof(struct pmem_desc));

	/* ポインタを返す */
	return (void *) ((uint32) desc.vaddr + 4096);
}

void crt_free(void *ptr)
{
	int err;
	struct pmem_desc *desc;

	/* 先頭に付加されたデスクリプタの位置を求める */
	desc = (struct pmem_desc *) ((uint32) ptr - 4096);

	/* ページを解放する */
	err = pmem_free((void *) desc);
	if(err != PMEM_SUCCESS)
		CRT_FATAL("pmem_pfree failed!");
}
