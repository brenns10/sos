/**
 * alloc_private.h: contains private declarations for the allocator
 *
 * If you're #including this, and you're not unit tests, you're doing things
 * wrong.
 */
#pragma once

#include <arch/arch.h>

#include "alloc.h"

/**
 * Zone descriptors manage zones of memory. The minimum zone that can be managed
 * by this mechanism is one 4096-byte page.
 *
 * Zone descriptors are kept in a sorted array (the zonelist), which may
 * dynamically expand and contract as needed. Each entry in the array is
 * (sizeof(void*)) bytes, containing the start address of the zone.
 */
struct zone {
#if defined(ARCH_32BIT)
	unsigned int addr : 20;         /* last 12 bits do not matter */
#elif defined(ARCH_64BIT)
	unsigned long long int addr : 52;
#else
	#error Need 64 or 32 bit kernel
#endif
	unsigned int _unallocated : 11; /* reserved for other uses */
	unsigned int free : 1;          /* boolean */
};
struct zonehdr {
#if defined(ARCH_32BIT)
	unsigned int next : 22;         /* last 10 bits do not matter */
#elif defined(ARCH_64BIT)
	unsigned long long next : 54;
#else
	#error Need 64 or 32 bit kernel
#endif
	unsigned int count : 10; /* number of zone structs */
	struct zone zones[];     /* array of zones */
};
