/*
 * Kernel CRT: memory allocation
 *
 * Simple first-fit allocator with a doubly-linked block list.
 * The heap is a single 8MB region in the BSS section.
 */

#include <sys/kcrt/kcrt.h>
#include <sys/hal/pmem.h>

/*
 * Heap Size
 */
#define HEAP_SIZE	(8 * 1024 * 1024)

/*
 * Alignment
 */
#define ALIGN		8
#define ALIGN_UP(x)	(((x) + (ALIGN - 1)) & ~(uint32)(ALIGN - 1))

/*
 * Block Magic
 */
#define BLOCK_MAGIC	0x4b424c4bU	/* "KBLK" */

/*
 * Block Header Size
 */
#define BLOCK_HDR_SIZE	ALIGN_UP(sizeof(struct block))

/*
 * Don't split a block if the remainder couldn't hold a header + minimum payload.
 */
#define MIN_SPLIT	(BLOCK_HDR_SIZE + ALIGN)

/*
 * Every block (used or free) starts with this header.
 * The payload follows immediately after it.
 */
struct block {
	uint32		magic;
	uint32		size;	/* Payload size in bytes (header excluded). */
	uint32		used;
	struct block	*prev;
	struct block	*next;
};

/*
 * The heap. (in BSS)
 */
static uint8 heap[HEAP_SIZE];

/*
 * Heap Head.
 */
static struct block *heap_head;

/*
 * Allocate the 8MB heap from pmem and set it up as one big free block.
 * Called lazily on the first malloc(); can also be called explicitly
 * during boot if deterministic init timing is preferred.
 */
static void heap_init(void)
{
	int err;

	heap_head = (struct block *)heap;
	heap_head->magic = BLOCK_MAGIC;
	heap_head->size = HEAP_SIZE - BLOCK_HDR_SIZE;
	heap_head->used = 0;
	heap_head->prev = NULL;
	heap_head->next = NULL;
}

void *malloc(size_t size)
{
	struct block	*b;
	struct block	*nb;
	uint32		asize;

	if(heap_head == NULL)
		heap_init();

	if(size == 0)
		return NULL;

	asize = ALIGN_UP((uint32) size);

	/* First fit. */
	for(b = heap_head; b != NULL; b = b->next) {
		if(b->used || b->size < asize)
			continue;

		/* Split the block if the remainder is worth keeping. */
		if(b->size >= asize + MIN_SPLIT) {
			nb = (struct block *) ((uint32) b + BLOCK_HDR_SIZE + asize);
			nb->magic = BLOCK_MAGIC;
			nb->size = b->size - asize - BLOCK_HDR_SIZE;
			nb->used = 0;
			nb->prev = b;
			nb->next = b->next;
			if(b->next != NULL)
				b->next->prev = nb;
			b->next = nb;
			b->size = asize;
		}

		b->used = 1;
		return (void *) ((uint32) b + BLOCK_HDR_SIZE);
	}

	/* Out of memory: the 8MB heap is exhausted. */
	return NULL;
}

void free(void *ptr)
{
	struct block *b;

	if(ptr == NULL)
		return;

	b = (struct block *) ((uint32) ptr - BLOCK_HDR_SIZE);

	if(b->magic != BLOCK_MAGIC)
		fatal("free: bad pointer!");
	if(!b->used)
		fatal("free: double free!");

	b->used = 0;

	/* Coalesce with the next block. */
	if(b->next != NULL && !b->next->used) {
		b->size += BLOCK_HDR_SIZE + b->next->size;
		b->next = b->next->next;
		if(b->next != NULL)
			b->next->prev = b;
	}

	/* Coalesce with the previous block. */
	if(b->prev != NULL && !b->prev->used) {
		b->prev->size += BLOCK_HDR_SIZE + b->size;
		b->prev->next = b->next;
		if(b->next != NULL)
			b->next->prev = b->prev;
	}
}
