/*
 * Utility functions
 */
#include "util.h"

/**
 * Return n, but aligned to the next boundary of "b" bits. That is, return the
 * first number greater than or equal to n, with the least significant b bits
 * being 0.
 */
uint32_t align(uint32_t n, uint32_t b)
{
	uint32_t mask = (1 << b) - 1; /* 3 -> 0x1000 -> 0x111 */

	if (n & mask)
		n += (1<<b) - n&mask;

	return n;
}
