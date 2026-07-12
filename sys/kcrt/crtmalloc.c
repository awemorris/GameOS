/*
 * Kernel CRT: memory allocation
 */

#include <sys/kcrt/kcrt.h>
#include <sys/hal/pmem.h>

void *malloc(size_t size)
{
	int		err;
	uint32	alloc_size;
	struct pmem_desc desc;

	/*
	 * FIXME: We do page allocation.
	 * TODO: Block allocation.
	 */

	/* Plus 1 page to add a descriptor to the head. */
	alloc_size = size + 4096;

	/*
	 * Allocate pages.
	 * _lo means the pages are linear-mapped to the system virtual address start.
	 */
	err = pmem_alloc_lo(alloc_size, &desc);
	if(err != PMEM_SUCCESS)
		fatal("pmem_palloc failed!");

	/* Add a descriptor. */
	memcpy((void *) desc.vaddr, &desc, sizeof(struct pmem_desc));

	return (void *) ((uint32) desc.vaddr + 4096);
}

void free(void *ptr)
{
	int err;
	struct pmem_desc *desc;

	desc = (struct pmem_desc *) ((uint32) ptr - 4096);

	err = pmem_free((void *) desc);
	if(err != PMEM_SUCCESS)
		fatal("pmem_pfree failed!");
}
