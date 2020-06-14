/*
 * kmalloc.c: A kernel memory allocator based on the slab allocator
 *
 * This allocator can allocate up to 2048 bytes of memory. Larger allocations
 * must be made with a page allocator or some other type of memory management
 * strategy.
 */

#include <stdint.h>

#include "kernel.h"
#include "slab.h"

struct kmalloc_size {
	int size;
	char *slabname;
	struct slab *slab;
};

struct kmalloc_size kmalloc_sizes[] = {
	/* Cannot have a slab for size 4 because it's smaller than llnode */
	{ 8, "kmalloc(8)", NULL },       { 16, "kmalloc(16)", NULL },
	{ 32, "kmalloc(32)", NULL },     { 64, "kmalloc(64)", NULL },
	{ 128, "kmalloc(128)", NULL },   { 256, "kmalloc(256)", NULL },
	{ 512, "kmalloc(512)", NULL },   { 1024, "kmalloc(1024)", NULL },
	{ 2048, "kmalloc(2048)", NULL },
};

static struct slab *find_slab(uint32_t size)
{
	int32_t i;

	for (i = 0; i < nelem(kmalloc_sizes); i++)
		if (kmalloc_sizes[i].size >= size)
			return kmalloc_sizes[i].slab;

	return NULL;
}

void *kmalloc(uint32_t size)
{
	struct slab *slab;

	slab = find_slab(size);
	if (!slab) {
		printf("kmalloc: Got allocation for size %u greater than "
		       "2048. Somebody didn't read the docs! Get ready for "
		       "a very interesting crash.\n",
		       size);
		return NULL;
	}
	return slab_alloc(slab);
}

void kfree(void *ptr, uint32_t size)
{
	struct slab *slab;

	slab = find_slab(size);
	if (!slab) {
		printf("kfree: Got free request for size %u greater than "
		       "2048. Very weird.\n",
		       size);
		return;
	}
	return slab_free(slab, ptr);
}

void kmalloc_init(void)
{
	uint32_t i;
	for (i = 0; i < nelem(kmalloc_sizes); i++) {
		kmalloc_sizes[i].slab =
		        slab_new(kmalloc_sizes[i].slabname,
		                 kmalloc_sizes[i].size, kmem_get_page);
	}
}
