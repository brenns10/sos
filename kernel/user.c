/*
 * Routines for handling userspace input.
 */
#include <stddef.h>

#include "kernel.h"
#include "string.h"
#include "sys/socket.h"

static int check_bounds(const void *user, size_t n)
{
	void *page;
	void *first = user;
	void *last = user + n - 1;

	if (umem_lookup_phys(current, first) == 0)
		return -EACCES;

	page = (void *)(((uint32_t)first & (~0xFFF)) + 0x1000);
	while (page < last) {
		if (umem_lookup_phys(current, page) == 0)
			return -EACCES;
		page = (void *)((uint32_t)page + 0x1000);
	}

	if (umem_lookup_phys(current, last) == 0)
		return -EACCES;

	return 0;
}

int copy_from_user(void *kerndst, const void *usersrc, size_t n)
{
	int rv = check_bounds(usersrc, n);
	if (rv < 0)
		return rv;
	memcpy(kerndst, usersrc, n);
	return 0;
}

int copy_to_user(void *userdst, const void *kernsrc, size_t n)
{
	int rv = check_bounds(userdst, n);
	if (rv < 0)
		return rv;
	memcpy(userdst, kernsrc, n);
	return 0;
}
