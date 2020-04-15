/*
 * Routines for handling userspace input.
 */
#include <stddef.h>

#include "kernel.h"
#include "string.h"
#include "sys/socket.h"

int copy_from_user(void *kerndst, void *usersrc, size_t n)
{
	if (usersrc < 0x40000000)
		return -EACCES;

	/* a more correct way to do this would be to check every page between
	 * start and end. we will do the sillier one */
	if (umem_lookup_phys(current, usersrc) == 0)
		return -EACCES;
	if (umem_lookup_phys(current, usersrc + n - 1) == 0)
		return -EACCES;

	memcpy(kerndst, usersrc, n);
	return 0;
}
