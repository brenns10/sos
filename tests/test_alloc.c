/*
 * test_alloc.c: test the page allocator routines
 */
#include <stdio.h>

#include "alloc.h"
#include "unittest.h"

#define FROM_LARGER

uint8_t allocator[PAGE_SIZE];

void init(struct unittest *test)
{
	init_page_allocator(allocator, 0x1000, 0x100000);
}

void test_first_fit(struct unittest *test)
{
	/*
	 * test that the allocator returns the first available memory region
	 * when there are no extra requirements (e.g. alignment)
	 */
	uint32_t allocated;
	
	init(test);
	allocated = alloc_pages(allocator, PAGE_SIZE, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x1000);

	allocated = alloc_pages(allocator, PAGE_SIZE, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x2000);
}

void test_free(struct unittest *test)
{
	/*
	 * test that freeing works in the stupidest of scenarios
	 */
	uint32_t allocated;
	
	init(test);
	allocated = alloc_pages(allocator, PAGE_SIZE, 0);
	UNITTEST_EXPECT_EQ(test, allocated, 0x1000);

	show_pages(allocator);

	free_pages(allocator, allocated, PAGE_SIZE);

	show_pages(allocator);

}

struct unittest_case cases[] = {
	UNITTEST_CASE(test_first_fit),
	UNITTEST_CASE(test_free),
	{0}
};

struct unittest_module module = {
	.name="lists",
	.cases=cases,
	.printf=printf,
};

UNITTEST(module);
