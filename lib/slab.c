/*
 * Slab allocator for frequently used structures. Built on top of a page
 * allocator. The first page allocated contains the struct slab, and the
 * remaining space is filled by structures. Subsequent pages contain only the
 * structures.
 */
#include <stdint.h>

#include "slab_private.h"

#define PAGE_SIZE 4096

static void slab_add_entries(struct slab *slab, void *page, unsigned int len)
{
	unsigned int i;
	struct list_head *tmp;

	for (i = 0; i + slab->size <= len; i += slab->size) {
		tmp = (struct list_head *)(page + i);
		list_insert_end(&slab->entries, tmp);
		slab->total += 1;
		slab->free += 1;
	}
}

struct slab *slab_new(unsigned int size, void *(*getter)(void))
{
	void *void_page = getter();
	struct slab *slab = void_page;
	unsigned int i;

	slab->size = size;
	slab->total = 0;
	slab->free = slab->total;
	slab->page_getter = getter;
	INIT_LIST_HEAD(slab->entries);

	slab_add_entries(slab, void_page + sizeof(struct slab),
	                 PAGE_SIZE - sizeof(struct slab));
	return slab;
}

void *slab_alloc(struct slab *slab)
{
	struct list_head *entry;

	/* Expand if necessary */
	if (slab->free == 0) {
		void *page = slab->page_getter();
		slab_add_entries(slab, page, PAGE_SIZE);
	}

	/* Return the first available entry from the first page */
	list_for_each(entry, &slab->entries)
	{
		slab->free -= 1;
		list_remove(entry);
		return (void *)entry;
	}
}

void slab_free(struct slab *slab, void *ptr)
{
	list_insert(&slab->entries, (struct list_head *)ptr);
	slab->free++;
}
