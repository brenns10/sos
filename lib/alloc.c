/**
 * alloc.c: allocates pages of memory, physical or virtual
 */
#include "alloc.h"
#include "alloc_private.h"

#define MAX_DESCRIPTORS 1023

/**
 * Declare this so we don't depend on any particular library providing printf,
 * whether it's the standard library or my own printf implementation...
 */
extern int printf(const char *fmt, ...);

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
 * This can fail if there's not enough room. Theoretically, we have a spot in
 * the zonehdr to point to a new page to store more zones, but hell if I'm going
 * to implement that.
 */
static bool shift_zones_up(struct zonehdr *hdr, int start, int to_shift)
{
	int i;

	if (hdr->count + start >= MAX_DESCRIPTORS)
		return false;

	for (i = hdr->count - 1; i >= start; i--)
		hdr->zones[i + to_shift] = hdr->zones[i];

	hdr->count += to_shift;
	return true;
}

static void shift_zones_down(struct zonehdr *hdr, int dst, int to_shift)
{
	int i;

	for (i = dst; i < hdr->count - to_shift; i++)
		hdr->zones[i] = hdr->zones[i + to_shift];

	hdr->count -= to_shift;
}

/**
 * Change the allocation status of region `exact` of length `count` within the
 * (potentially larger) region `region`.
 */
static bool change_status(
	struct zonehdr *hdr, uint32_t exact, uint32_t count, uint32_t region,
	uint32_t index, int status
)
{
	int to_insert = 0;
	uint32_t next = 0;

	bool have_left_zone = (index > 0);
	bool have_right_zone = (index < hdr->count - 1);
	bool exact_on_left=false, exact_on_right=false;

	if (have_right_zone) {
		next = hdr->zones[index+1].addr << PAGE_BITS;
		exact_on_right = (exact + count == next);
	}
	exact_on_left = (region == exact);

	if (exact_on_left && exact_on_right) {
		/* there must be a right zone */
		if (have_left_zone) {
			/*
			 * aaa  BBB  aaa
			 *
			 * We delete the last two and shift everything after
			 * down by two:
			 *
			 * aaaAAAaaa
			 *
			 * (test_free)
			 */
			shift_zones_down(hdr, index, 2);
		} else {
			/*
			 * BBB aaa
			 *
			 * Shift everything down by one and modify the AAA zone
			 * to have our exact left address:
			 *
			 * AAAAAA
			 */
			shift_zones_down(hdr, index, 1);
			hdr->zones[index].addr = (exact>>PAGE_BITS);
		}
	} else if (exact_on_left) {
		/*
		 * ?a? BBBbb (aaa)
		 *
		 * We move the current zone up to (exact+count). If
		 * there's a left zone, we're set. Otherwise, we insert
		 * one and set it to the correct status. We don't care if
		 * there's a right zone.
		 *
		 * ?a?AAA bb AAA
		 */
		if (have_left_zone) {
			hdr->zones[index].addr = ((exact+count)>>PAGE_BITS);
		} else {
			if (!shift_zones_up(hdr, index, 1))
				return false;
			hdr->zones[index+1].addr = ((exact+count)>>PAGE_BITS);
			hdr->zones[index].free = status;
		}
	} else if (exact_on_right) {
		/*
		 * ?a? bbBBB aaa
		 *
		 * We know there's a right zone, we just slide that one down.
		 *
		 * ?a? bb AAAaaa
		 */
		hdr->zones[index+1].addr = (exact>>PAGE_BITS);
	} else {
		/*
		 * ?a? bbBBBbb ?a?
		 *
		 * We must shift up two, insert a descriptor for the new AAA
		 * zone, and a descriptor for the right side bb zone.
		 *
		 * ?a? bb AAA bb ?a?
		 */
		if (!shift_zones_up(hdr, index, 2))
			return false;
		hdr->zones[index+1].addr = (exact >> PAGE_BITS);
		hdr->zones[index+1].free = status;
		hdr->zones[index+2].addr = ((exact+count) >> PAGE_BITS);
		hdr->zones[index+2].free = hdr->zones[index].free;
	}
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
uint32_t alloc_pages(void *allocator, uint32_t count, uint32_t align)
{
	uint32_t i, align_mask, zone, alignzone, next;
	struct zonehdr *hdr = (struct zonehdr*) allocator;
	bool result = false;

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
		if (alignzone + count > next) {
			/* can't satisfy alignment and/or size in zone */
			continue;
		}

		result = change_status(
			hdr, alignzone, count, zone, i, 0
		);
		if (result) {
			return alignzone;
		}
	}

	return 0;
}

bool free_pages(void *allocator, uint32_t start, uint32_t count)
{
	uint32_t i, addr, next;
	bool has_next;
	struct zonehdr *hdr = (struct zonehdr *)allocator;

	for (i = 0; i < hdr->count; i++) {
		addr = hdr->zones[i].addr << PAGE_BITS;
		has_next = i+1 < hdr->count;

		/* Trying to free a region from before the allocator's memory. */
		if (addr > start)
			return false;

		/* Compute next */
		if (has_next)
			next = hdr->zones[i+1].addr << PAGE_BITS;

		/* If the zone doesn't fit within this block, continue */
		if (has_next && start + count > next)
			continue;

		/* already freed, or never even allocated */
		if (hdr->zones[i].free)
			return false;

		return change_status(
			hdr, start, count, addr, i, 1);
	}
	return false;
}

bool mark_alloc(void *allocator, uint32_t start, uint32_t count)
{
	uint32_t i, addr, next;
	bool has_next;
	struct zonehdr *hdr = (struct zonehdr *)allocator;

	for (i = 0; i < hdr->count; i++) {
		addr = hdr->zones[i].addr << PAGE_BITS;
		has_next = i+1 < hdr->count;

		/* Trying to free a region from before the allocator's memory. */
		if (addr > start)
			return false;

		/* Compute next */
		if (has_next)
			next = hdr->zones[i+1].addr << PAGE_BITS;

		/* If the zone doesn't fit within this block, continue */
		if (has_next && start + count > next)
			continue;

		/* already allocated! */
		if (!hdr->zones[i].free)
			return false;

		return change_status(
			hdr, start, count, addr, i, 0);
	}
	return false;
}
