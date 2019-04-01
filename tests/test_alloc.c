/*
 * test_alloc.c: test the page allocator routines
 */
#include <stdio.h>

#include "alloc_private.h"
#include "unittest.h"

#define FREE 1
#define ALLOC 0
#define ZONE(addr_, free_) {.addr=(addr_)>>PAGE_BITS, .free=free_}
uint8_t allocator[PAGE_SIZE];

void init(struct unittest *test)
{
	init_page_allocator(allocator, 0x1000, 0x100000);
}

void expect_zones(struct unittest *test, struct zone zones[])
{
	int i;
	struct zonehdr *hdr = (struct zonehdr *) allocator;
	for (i = 0; zones[i].addr != 0; i++) {
		UNITTEST_EXPECT_EQ(test, hdr->zones[i].addr, zones[i].addr);
		UNITTEST_EXPECT_EQ(test, hdr->zones[i].free, zones[i].free);
	}
	UNITTEST_EXPECT_EQ(test, hdr->count, i);
}

void test_first_fit(struct unittest *test)
{
	/*
	 * Test that the allocator returns the first available memory region
	 * when there are no extra requirements (e.g. alignment)
	 */
	uint32_t allocated;
	
	init(test);
	allocated = alloc_pages(allocator, PAGE_SIZE, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x1000);
}

void test_combine_adjacent(struct unittest *test)
{
	/*
	 * Test that when we allocate two zones next to each other, they get
	 * lumped into the same zone descriptor.
	 */
	uint32_t allocated;
	init(test);
	alloc_pages(allocator, PAGE_SIZE, 0);
	alloc_pages(allocator, PAGE_SIZE, 0);

	struct zone zones[] = {
		ZONE(0x1000, ALLOC),
		ZONE(0x3000, FREE),
		ZONE(0X100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_alignment(struct unittest *test)
{
	/*
	 * Test that we can get memory aligned to our specification.
	 */
	uint32_t allocated;

	init(test);
	allocated = alloc_pages(allocator, PAGE_SIZE, PAGE_BITS + 1);
	UNITTEST_EXPECT_EQ(test, allocated, 0x2000);

	struct zone zones[] = {
		ZONE(0x1000, FREE),
		ZONE(0x2000, ALLOC),
		ZONE(0x3000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_skip_hole(struct unittest *test)
{
	/*
	 * Test that, when the first free zone is a hole that is too small, we
	 * skip it and return a zone that fits.
	 */
	uint32_t allocated;

	init(test);
	alloc_pages(allocator, PAGE_SIZE * 3, 0); /* 0x1000 - 0x4000 */
	free_pages(allocator, 0x2000, PAGE_SIZE); /* 0x2000 - 0x3000 */

	/* the first gap is not big enough, have to use 0x4000 */
	allocated = alloc_pages(allocator, PAGE_SIZE * 2, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x4000);

	struct zone zones[] = {
		ZONE(0x1000, ALLOC),
		ZONE(0x2000, FREE),
		ZONE(0x3000, ALLOC),
		ZONE(0x6000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_exact_hole(struct unittest *test)
{
	/*
	 * Test that, when the first free zone is a hole that is perfectly
	 * sized, we return that.
	 */
	uint32_t allocated;

	init(test);
	alloc_pages(allocator, PAGE_SIZE * 3, 0); /* 0x1000 - 0x4000 */
	free_pages(allocator, 0x2000, PAGE_SIZE); /* 0x2000 - 0x3000 */

	/* we should be able to use 0x2000 */
	allocated = alloc_pages(allocator, PAGE_SIZE, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x2000);

	struct zone zones[] = {
		ZONE(0x1000, ALLOC),
		ZONE(0x4000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_free(struct unittest *test)
{
	/*
	 * Test that freeing works in a basic scenario
	 */
	uint32_t allocated;
	bool result;
	
	init(test);
	allocated = alloc_pages(allocator, PAGE_SIZE, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x1000);

	result = free_pages(allocator, allocated, PAGE_SIZE);
	UNITTEST_EXPECT_EQ(test, result, true);

	struct zone zones[] = {
		ZONE(0x1000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_free_after_free_region(struct unittest *test)
{
	/*
	 * Test freeing when it's not the first region in the list.
	 */
	uint32_t allocated;
	bool result;
	
	init(test);
	/* should get 0x2000 - 0x6000 */
	allocated = alloc_pages(allocator, PAGE_SIZE * 4, PAGE_BITS + 1);
	UNITTEST_EXPECT_EQ(test, allocated, 0x2000);

	result = free_pages(allocator, 0x3000, PAGE_SIZE);
	UNITTEST_EXPECT_EQ(test, result, true);

	result = free_pages(allocator, 0x5000, PAGE_SIZE);
	UNITTEST_EXPECT_EQ(test, result, true);

	struct zone zones[] = {
		ZONE(0x1000, FREE),
		ZONE(0x2000, ALLOC),
		ZONE(0x3000, FREE),
		ZONE(0x4000, ALLOC),
		ZONE(0x5000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);

	result = free_pages(allocator, 0x4000, PAGE_SIZE);
	UNITTEST_EXPECT_EQ(test, result, true);

	result = free_pages(allocator, 0x2000, PAGE_SIZE);
	UNITTEST_EXPECT_EQ(test, result, true);

	struct zone final_zones[] = {
		ZONE(0x1000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, final_zones);
}

void test_alloc_whole_thing(struct unittest *test)
{
	/*
	 * Weird scenario, make sure that allocating and freeing the whole
	 * memory region works.
	 */
	uint32_t allocated;

	init(test);
	allocated = alloc_pages(allocator, 0x100000 - 0x1000, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x1000);

	free_pages(allocator, allocated, 0x100000 - 0x1000);

	struct zone zones[] = {
		ZONE(0x1000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_free_exact_on_right_but_not_left(struct unittest *test)
{
	/*
	 * When you have an allocated region and you free the right half of it,
	 * leaving the left half still allocated.
	 */
	uint32_t allocated;

	init(test);
	alloc_pages(allocator, 0x2000, 0); /* gives me 0x1000-0x3000 */
	free_pages(allocator, 0x2000, 0x1000); /* free 0x2000-0x3000 */

	struct zone zones[] = {
		ZONE(0x1000, ALLOC),
		ZONE(0x2000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_unsatisfiable_allocation(struct unittest *test)
{
	/*
	 * Try allocating something that can't be allocated, should receive 0.
	 */
	uint32_t allocated;

	init(test);
	allocated = alloc_pages(allocator, 0x100000, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0);
}

void test_unsatisfiable_free(struct unittest *test)
{
	/*
	 * Try freeing more memory than we actually received. free_pages()
	 * should check to make sure that the freed memory fits within an
	 * allocated region, otherwise it will corrupt the memory zone
	 * descriptors.
	 *
	 * Of course, if an application is freeing memory it was never
	 * allocated, there's no saving it anyway, shrug.
	 */
	bool result;

	init(test);
	alloc_pages(allocator, 0x1000, 0); /* 0x1000 - 0x2000 */
	result = free_pages(allocator, 0x1000, 0x10000);
	UNITTEST_EXPECT_EQ(test, result, false);

	struct zone zones[] = {
		ZONE(0x1000, ALLOC),
		ZONE(0x2000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_free_already_freed(struct unittest *test)
{
	bool result;
	init(test);
	result = free_pages(allocator, 0x1000, 0x1000);
	UNITTEST_EXPECT_EQ(test, result, false);

	struct zone zones[] = {
		ZONE(0x1000, FREE),
		ZONE(0x100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

void test_mark_alloc(struct unittest *test)
{
	/*
	 * We should be able to take any region which isn't already allocated,
	 * and make it our own.
	 */
	bool result;
	init(test);

	result = mark_alloc(allocator, 0x1000, 0x1000);
	UNITTEST_EXPECT_EQ(test, result, true);

	result = mark_alloc(allocator, 0x2000, 0x1000);
	UNITTEST_EXPECT_EQ(test, result, true);

	result = mark_alloc(allocator, 0x4000, 0x1000);
	UNITTEST_EXPECT_EQ(test, result, true);

	result = mark_alloc(allocator, 0x100000-0x1000, 0x1000);
	UNITTEST_EXPECT_EQ(test, result, true);

	struct zone zones[] = {
	 ZONE(0x1000, ALLOC),
	 ZONE(0x3000, FREE),
	 ZONE(0x4000, ALLOC),
	 ZONE(0x5000, FREE),
	 ZONE(0x100000-0x1000, ALLOC),
	 {}
	};
	expect_zones(test, zones);
}

void test_mark_alloc_already_alloced(struct unittest *test)
{
	/*
	 * If we try to allocated memory that is already allocated, it will
	 * fail.
	 */
	bool result;
	init(test);

	result = mark_alloc(allocator, 0x2000, 0x2000);
	UNITTEST_EXPECT_EQ(test, result, true);

	result = mark_alloc(allocator, 0x2000, 0x3000);
	UNITTEST_EXPECT_EQ(test, result, false);

	result = mark_alloc(allocator, 0x1000, 0x3000);
	UNITTEST_EXPECT_EQ(test, result, false);

	result = mark_alloc(allocator, 0x1000, 0x4000);
	UNITTEST_EXPECT_EQ(test, result, false);

	result = mark_alloc(allocator, 0x2000, 0x1000);
	UNITTEST_EXPECT_EQ(test, result, false);

	struct zone zones[] = {
		ZONE(0x1000, FREE),
		ZONE(0x2000, ALLOC),
		ZONE(0x4000, FREE),
		ZONE(0X100000, ALLOC),
		{}
	};
	expect_zones(test, zones);
}

struct unittest_case cases[] = {
	UNITTEST_CASE(test_first_fit),
	UNITTEST_CASE(test_combine_adjacent),
	UNITTEST_CASE(test_alignment),
	UNITTEST_CASE(test_skip_hole),
	UNITTEST_CASE(test_exact_hole),
	UNITTEST_CASE(test_free),
	UNITTEST_CASE(test_free_after_free_region),
	UNITTEST_CASE(test_alloc_whole_thing),
	UNITTEST_CASE(test_free_exact_on_right_but_not_left),
	UNITTEST_CASE(test_unsatisfiable_allocation),
	UNITTEST_CASE(test_unsatisfiable_free),
	UNITTEST_CASE(test_free_already_freed),
	UNITTEST_CASE(test_mark_alloc),
	UNITTEST_CASE(test_mark_alloc_already_alloced),
	{0}
};

struct unittest_module module = {
	.name="lists",
	.cases=cases,
	.printf=printf,
};

UNITTEST(module);
