/**
 * pages.c - manages pages of physical memory
 */
#include "kernel.h"

#define PAGE_SIZE 4096
#define PAGE_BITS 12
#define MAX_DESCRIPTORS 1023

/**
 * Zone descriptors manage zones of memory. The minimum zone that can be managed
 * by this mechanism is one 4096-byte page.
 *
 * Zone descriptors are kept in a sorted array (the zonelist), which may
 * dynamically expand and contract as needed. Each entry in the array is 4
 * bytes, containing the start address of the zone.
 */
struct zone {
	unsigned int addr : 20;        /* last 12 bits do not matter */
	unsigned int _unallocated: 11; /* reserved for other uses */
	unsigned int free : 1;         /* boolean */
};
struct zonehdr {
	unsigned int next : 22;        /* last 10 bits do not matter */
	unsigned int count : 10;       /* number of zone structs */
	struct zone zones[];           /* array of zones */
};

/**
 * Set up the page list.
 * allocator: Pointer to 4096-byte page which holds allocator itself
 * start: start of range to manage
 * end: end of range to manage (exclusive)
 */
void init_page_allocator(void *allocator, uint32_t start, uint32_t end)
{
	/*
	 * We assume there are 3GB of available memory, starting at 0x40000000.
	 * Everything beginning at dynamic_start and ending at 0xFFFFFFFF is
	 * assumed to be available for us to allocate.
	 */
	struct zonehdr *zonehdr = (struct zonehdr*) allocator;
	zonehdr->next = 0;
	zonehdr->count = 2;
	zonehdr->zones[0].addr = start >> PAGE_BITS;
	zonehdr->zones[0].free = 1;
	zonehdr->zones[1].addr = end >> PAGE_BITS;
	zonehdr->zones[1].free = 0;
}

void show_pages(void *allocator)
{
	struct zonehdr *hdr = (struct zonehdr*) allocator;
	int i;

	printf("BEGIN MEMORY ZONES (%u)\n", hdr->count);
	for (i = 0; i < hdr->count; i++)
		printf(" 0x%x: %s\n", hdr->zones[i].addr << PAGE_BITS,
				hdr->zones[i].free ? "FREE" : "ALLOCATED");
	printf("END MEMORY ZONES\n");
}

/**
 * Move zones to the right by `to_shift` descriptors, starting at `start`.
 */
bool shift_zones_up(struct zonehdr *hdr, int start, int to_shift)
{
	int i;

	if (hdr->count + start >= MAX_DESCRIPTORS)
		return false;

	for (i = hdr->count - 1; i >= start; i--)
		hdr->zones[i + to_shift] = hdr->zones[i];

	return true;
}

/**
 * Allocate physical pages.
 * count: how many bytes to allocate
 * align: what byte boundary to align on?
 *   <12: default, 4KB aligned
 *   13: 8KB aligned
 *   14: 16KB aligned, etc
 * return: physical pointer to contiguous pages
 *   NULL if the memory could not be allocated
 */
void *alloc_pages(void *allocator, uint32_t count, uint32_t align)
{
	uint32_t i, align_mask, zone, alignzone, next;
	int to_insert;
	struct zonehdr *hdr = (struct zonehdr*) allocator;

	/* threshold alignment between PAGE_BITS <= align <= 32 */
	align = (align < PAGE_BITS ? PAGE_BITS : align);
	align = (align > 32 ? 32 : align);

	/* align mask has the N least significant bits set */
	align_mask = 0xFFFFFFFF >> (32 - align);

	for (i = 0; i < hdr->count; i++) {
		if (!hdr->zones[i].free) continue;
		alignzone = zone = hdr->zones[i].addr << PAGE_BITS;

		/* align memory if necessary */
		if (zone & align_mask) {
			alignzone = ((zone >> align) + 1) << align;
		}

		next = hdr->zones[i + 1].addr << PAGE_BITS;
		if (alignzone + count >= next) {
			/* can't satisfy alignment and/or size in zone */
			continue;
		}

		/*
		 * At this point we have:
		 *   zone: beginning of free zone
		 *   alignzone: beginning of area we'll allocate
		 *   alignzone + count: start of next free zone
		 * Further, we know the allocation will fit within the zone.
		 * Finally, we know the zones on either side of this one MUST
		 * NOT be free (otherwise, they would be merged with this zone).
		 *
		 * Either the allocation fits perfectly, or we have created
		 * "free zone holes" on either (or both) side of this zone.
		 */
		to_insert = 0;
		if (zone != alignzone)
			to_insert += 1;
		if (alignzone + count != next)
			to_insert += 1;

		/* Shift zone descriptors over, or fail. */
		if (!shift_zones_up(hdr, i, to_insert))
			continue;

		if (zone != alignzone) {
			hdr->zones[i].addr = zone >> PAGE_BITS;
			hdr->zones[i].free = 1;
			i += 1;
		}
		hdr->zones[i].addr = alignzone >> PAGE_BITS;
		hdr->zones[i].free = 0;
		i += 1;
		if (alignzone + count != next) {
			hdr->zones[i].addr = (alignzone + count) >> PAGE_BITS;
			hdr->zones[i].free = 1;
		}
		hdr->count += to_insert;
		return (void*) alignzone;
	}

	return NULL;
}
