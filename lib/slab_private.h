#pragma once

#include "list.h"
#include "slab.h"

struct slab {
	unsigned int size;        /* size of structure */
	unsigned int total;       /* count of structures in total */
	unsigned int free;        /* count which are free */
	char *name;               /* name of this slab allocator, diagnostic */
	struct list_head entries; /* list of pages */
	struct list_head slabs;   /* list of slab allocators */

	void *(*page_getter)(void);
};
