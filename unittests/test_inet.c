/*
 * test_slab.c: test the slab allocator routines
 */
#include <stdbool.h>
#include <stdio.h>

#include "inet.h"
#include "unittest.h"

static inline void atonvalid(struct unittest *test, char *cp, uint32_t exp)
{
	uint32_t addr;
	int result;
	result = inet_aton(cp, &addr);
	UNITTEST_EXPECT_EQ(test, result, 1);
	UNITTEST_EXPECT_EQ(test, addr, exp);
}

static inline void atonbad(struct unittest *test, char *cp)
{
	uint32_t addr;
	int result;
	result = inet_aton(cp, &addr);
	UNITTEST_EXPECT_EQ(test, result, 0);
}

void test_aton_correct(struct unittest *test)
{
	atonvalid(test, "255.0.0.255", 0xFF0000FF);
	atonvalid(test, "1.2.3.4", 0x04030201);
}

void test_aton_incorrect(struct unittest *test)
{
	atonbad(test, "");
	atonbad(test, "1.2.3.4.5");
	atonbad(test, "1.2.3");
	atonbad(test, "foo");
	atonbad(test, "1.bar");
	atonbad(test, "1.2.3.1234");
	atonbad(test, "1234.2.3.4");
	atonbad(test, "1..3.4");
	atonbad(test, "1.2.3.");
}

struct unittest_case cases[] = {
	UNITTEST_CASE(test_aton_correct),
	UNITTEST_CASE(test_aton_incorrect),
	{ 0 },
};

struct unittest_module module = {
	.name = "inet",
	.cases = cases,
	.printf = printf,
};

UNITTEST(module);
