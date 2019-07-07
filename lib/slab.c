/*
 * Slab allocator for frequently used structures. Built on top of a page
 * allocator. Supports sizes, but not alignment.
 *
 * Each page of memory contains a header for a linked list joining them all
 * together, as well as allocation counts within the page. The first page
 * further contains a struct slab, which has overall bookkeeping for the
 * allocator.
 */
#include <stdint.h>

#include "slab_private.h"

#define PAGE_SIZE 4096
#define PAGE_MASK (~0xFFF)

static void slab_page_create_entries(struct slab_page *page, unsigned int offset)
{
	unsigned int i;
	struct list_head *tmp;
	INIT_LIST_HEAD(page->entries);

	for (i = 0; i < page->total; i++) {
		tmp = (struct list_head*)((void*)page + offset + page->slab->size * i);
		list_insert_end(&page->entries, tmp);
	}
}

struct slab *slab_new(unsigned int size, void *(*getter)(void), void (*freer)(void*))
{
	void *void_page = getter();
	struct slab *slab = void_page + sizeof(struct slab_page);
	struct slab_page *page = void_page;
	struct list_head *tmp;
	unsigned int i;

	slab->size = size;
	slab->total = (PAGE_SIZE - sizeof(struct slab) - sizeof(struct slab_page)) / size;
	slab->free = slab->total;
	slab->page_getter = getter;
	slab->page_freer = freer;

	page->slab = slab;
	page->total = slab->total;
	page->free = slab->free;

	INIT_LIST_HEAD(slab->pages);
	list_insert(&slab->pages, &page->pages);

	slab_page_create_entries(page, sizeof(struct slab) + sizeof(struct slab_page));
	return slab;
}

void *slab_alloc(struct slab *slab)
{
	struct slab_page *page;
	struct list_head *entry;
	if (slab->free > 0) {
		/* Return the first available entry from the first page */
		list_for_each_entry(page, &slab->pages, pages, struct slab_page) {
			list_for_each(entry, &page->entries) {
				page->free -= 1;
				slab->free -= 1;
				list_remove(entry);
				return (void*)entry;
			}
		}
	} else {
		/* TODO need to expand the cache */
		return NULL;
	}
}

void slab_free(void *ptr)
{
	struct slab_page *page = (struct slab_page *)((uintptr_t)ptr & PAGE_MASK);
	list_insert(&page->entries, ptr);
	page->free++;
	page->slab->free++;
}
