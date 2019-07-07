/*
 * test_slab.c: test the slab allocator routines
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "slab_private.h"
#include "unittest.h"

int pages_allocd;

/* our "pages" must be aligned like the ones we would allocated in SOS */
uint8_t first_page[4096] __attribute__ ((aligned(4096)));
uint8_t second_page[4096] __attribute__ ((aligned(4096)));

bool first_page_freed, second_page_freed;

struct slab *slab;

void *page_getter(void) {
	pages_allocd++;
	if (pages_allocd == 1) {
		return first_page;
	} else {
		return second_page;
	}
}

void page_freer(void *page) {
	if (page == (void*) first_page) {
		first_page_freed = true;
	} else {
		second_page_freed = true;
	}
}

void init(struct unittest *test)
{
	pages_allocd = 0;
	first_page_freed = false;
	second_page_freed = false;
}

void test_allocates(struct unittest *test)
{
	void *alloc, *expected;
	init(test);
	slab = slab_new(64, page_getter, page_freer);
	alloc = slab_alloc(slab);

	expected = (void*)first_page + sizeof(struct slab) + sizeof(struct slab_page);
	UNITTEST_EXPECT_EQ(test, alloc, expected);

	alloc = slab_alloc(slab);
	expected = expected + 64;
	UNITTEST_EXPECT_EQ(test, alloc, expected);
}

void test_frees(struct unittest *test)
{
	void *alloc1, *alloc2, *alloc3;
	init(test);
	slab = slab_new(64, page_getter, page_freer);
	alloc1 = slab_alloc(slab);
	alloc2 = slab_alloc(slab);
	slab_free(alloc2);
	alloc3 = slab_alloc(slab);
	UNITTEST_EXPECT_EQ(test, alloc2, alloc3);
}

struct unittest_case cases[] = {
	UNITTEST_CASE(test_allocates),
	UNITTEST_CASE(test_frees),
	{0}
};

struct unittest_module module = {
	.name="slab",
	.cases=cases,
	.printf=printf,
};

UNITTEST(module);
