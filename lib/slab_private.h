#pragma once

#include "list.h"
#include "slab.h"

struct slab {
	unsigned int size;      /* size of structure */
	unsigned int total;     /* count of structures in total */
	unsigned int free;      /* count which are free */
	struct list_head pages; /* list of pages */

	void *(*page_getter)(void);
	void (*page_freer)(void*);
};

struct slab_page {
	struct slab *slab;
	unsigned int total;
	unsigned int free;
	struct list_head pages;
	struct list_head entries;
};
