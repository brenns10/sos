/*
 * inet.c: IP related utilities to share with userspace
 */
#include <stdbool.h>
#include <stdint.h>

#include "inet.h"

uint32_t ntohl(uint32_t orig)
{
	return ((orig & 0xFF) << 24) | ((orig & 0xFF00) << 8) |
	       ((orig & 0xFF0000) >> 8) | ((orig & 0xFF000000) >> 24);
}

uint16_t ntohs(uint16_t orig)
{
	return ((orig & 0xFF) << 8 | (orig & 0xFF00) >> 8);
}

uint32_t htonl(uint32_t orig)
{
	return ntohl(orig);
}

uint32_t htons(uint16_t orig)
{
	return ntohs(orig);
}

/*
 * Unlike traditional inet_aton, we only support 4-component addresses.
 */
int inet_aton(const char *cp, uint32_t *addr)
{
	uint32_t myaddr = 0;
	uint32_t component = 0;
	int group = 0;
	bool seen_in_group = false;
	int i = 0;

	for (i = 0; cp[i]; i++) {
		if (cp[i] >= '0' && cp[i] <= '9') {
			component = component * 10 + (cp[i] - '0');
			seen_in_group = true;
		} else if (cp[i] == '.') {
			if (component > 255)
				return 0;
			if (group >= 3)
				return 0;
			if (!seen_in_group)
				return 0;
			myaddr |= component << (8 * group);
			group++;
			component = 0;
			seen_in_group = false;
		} else {
			return 0;
		}
	}
	if (component > 255)
		return 0;
	if (group != 3)
		return 0;
	if (!seen_in_group)
		return 0;
	myaddr |= component << (8 * group);
	*addr = myaddr;
	return 1;
}
