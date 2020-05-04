/*
 * test_slab.c: test the slab allocator routines
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "format.h"
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

void test_formats_hex(struct unittest *test)
{
	idx = 0;
	test_printf("foo=%x\n", 5);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=5\n"), 0);

	idx = 0;
	test_printf("foo=%x\n", 11);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=b\n"), 0);

	idx = 0;
	test_printf("foo=%x\n", 3735928559);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=deadbeef\n"), 0);

	/* test leading and non-leading zeros */
	idx = 0;
	test_printf("foo=%x\n", 0x0010101);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=10101\n"), 0);

	idx = 0;
	test_printf("foo=%x\n", 0);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "foo=0\n"), 0);
}

void test_formats_mac(struct unittest *test)
{
	uint8_t mac[6] = { 0xAB, 0x12, 0x45, 0xEF, 0x89, 0x7C };
	idx = 0;
	test_printf("a mac=%M is here", &mac);
	UNITTEST_EXPECT_EQ(
	        test, strcmp(buffer, "a mac=ab:12:45:ef:89:7c is here"), 0);
}

void test_formats_ipv4(struct unittest *test)
{
	struct in_addr inet;
	int rv = inet_aton("192.168.0.1", &inet);
	UNITTEST_EXPECT_EQ(test, rv, 1);
	idx = 0;
	test_printf("IP=%I\n", inet.s_addr);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "IP=192.168.0.1\n"), 0);
}

void test_formats_string(struct unittest *test)
{
	idx = 0;
	test_printf("Hello, %s!", "world");
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "Hello, world!"), 0);
}

void test_limited_space(struct unittest *test)
{
	char buf[10];
	test_snprintf(buf, sizeof(buf), "%x %s !", 0xdead, "abcde");
	UNITTEST_EXPECT_EQ(test, strcmp(buf, "dead abcd"), 0);
}

void test_edge(struct unittest *test)
{
	idx = 0;
	test_printf("A %s %%", "literal");
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "A literal %"), 0);

	idx = 0;
	test_printf("%s don't break %", "percent");
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "percent don't break %"), 0);

	idx = 0;
	test_printf("%x is hex, but %y?", 17);
	UNITTEST_EXPECT_EQ(test, strcmp(buffer, "11 is hex, but %y?"), 0);
}

void test_atoi_basic(struct unittest *test)
{
	/* atoi is famously bad function, tests here are thus not thorough */
	UNITTEST_EXPECT_EQ(test, test_atoi("123"), 123);
	UNITTEST_EXPECT_EQ(test, test_atoi("0"), 0);
	UNITTEST_EXPECT_EQ(test, test_atoi("09"), 9);
}

struct unittest_case cases[] = {
	UNITTEST_CASE(test_formats_uint),  UNITTEST_CASE(test_formats_int),
	UNITTEST_CASE(test_formats_hex),   UNITTEST_CASE(test_formats_mac),
	UNITTEST_CASE(test_formats_ipv4),  UNITTEST_CASE(test_formats_string),
	UNITTEST_CASE(test_limited_space), UNITTEST_CASE(test_edge),
	UNITTEST_CASE(test_atoi_basic),    { 0 },
};

struct unittest_module module = {
	.name = "format",
	.cases = cases,
	.printf = printf,
};

UNITTEST(module);
