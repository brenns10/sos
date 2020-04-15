/*
 * test_slab.c: test the slab allocator routines
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unittest.h"

char buffer[4096];
char idx = 0;

void test_puts(char *string)
{
	strcpy(buffer + idx, string);
	idx += strlen(string);
}

void test_formats_uint(struct unittest *test)
{
	idx = 0;
	test_printf("foo=%u", 5);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=5"), 0);

	idx = 0;
	test_printf("foo=%u", 0xFFFFFFFF);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=4294967295"), 0);
}

void test_formats_int(struct unittest *test)
{
	idx = 0;
	test_printf("foo=%d", 5);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=5"), 0);

	idx = 0;
	test_printf("foo=%d", 0xFFFFFFFF);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=-1"), 0);

	idx = 0;
	test_printf("foo=%d", -123);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=-123"), 0);
}

struct unittest_case cases[] = {
	UNITTEST_CASE(test_formats_uint),
	UNITTEST_CASE(test_formats_int),
	{ 0 },
};

struct unittest_module module = {
	.name = "format",
	.cases = cases,
	.printf = printf,
};

UNITTEST(module);
