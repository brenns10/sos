/*
 * Slab allocator for frequently used structures. Built on top of a page
 * allocator. The first page allocated contains the struct slab, and the
 * remaining space is filled by structures. Subsequent pages contain only the
 * structures.
 */
#include <stdint.h>

#include "slab_private.h"

#define PAGE_SIZE 4096

/**
 * Declare this so we don't depend on any particular library providing printf,
 * whether it's the standard library or my own printf implementation...
 */
extern int printf(const char *fmt, ...);

DECLARE_LIST_HEAD(slabs);

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

struct slab *slab_new(char *name, unsigned int size, void *(*getter)(void))
{
	void *void_page = getter();
	struct slab *slab = void_page;

	if (size < sizeof(struct list_head)) {
		printf("slab: invalid slab size %u smaller than llnode %u\n",
		       size, sizeof(struct list_head));
		return NULL;
	}

	slab->size = size;
	slab->total = 0;
	slab->free = slab->total;
	slab->page_getter = getter;
	INIT_LIST_HEAD(slab->entries);
	list_insert_end(&slabs, &slab->slabs);
	slab->name = name;

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

	return NULL;
}

void slab_free(struct slab *slab, void *ptr)
{
	list_insert(&slab->entries, (struct list_head *)ptr);
	slab->free++;
}

void slab_report(struct slab *slab)
{
	int headerct, headerwaste, regct, regwaste, pages;
	printf(" slab \"%s\":\n", slab->name);
	printf("  item_size %u\n  %u alloc / %u total (%u free)\n", slab->size,
	       slab->total - slab->free, slab->total, slab->free);
	headerct = (PAGE_SIZE - sizeof(struct slab)) / slab->size;
	regct = PAGE_SIZE / slab->size;
	headerwaste = PAGE_SIZE - sizeof(struct slab) - slab->size * headerct;
	regwaste = PAGE_SIZE - slab->size * regct;
	printf("  header fits %u structures, wasting %u bytes\n", headerct,
	       headerwaste);
	printf("  regular page fits %u structures, wasting %u bytes\n", regct,
	       regwaste);
	pages = (slab->total - headerct) / regct + 1;
	printf("  %u pages (including header) allocated = %u bytes\n", pages,
	       pages * PAGE_SIZE);
}

void slab_report_all(void)
{
	struct slab *slab;
	list_for_each_entry(slab, &slabs, slabs)
	{
		slab_report(slab);
	}
}
