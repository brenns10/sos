/**
 * alloc_private.h: contains private declarations for the allocator
 *
 * If you're #including this, and you're not unit tests, you're doing things
 * wrong.
 */
#pragma once

#include "alloc.h"

#define MAX_DESCRIPTORS 1023

/**
 * Zone descriptors manage zones of memory. The minimum zone that can be managed
 * by this mechanism is one 4096-byte page.
 *
 * Zone descriptors are kept in a sorted array (the zonelist), which may
 * dynamically expand and contract as needed. Each entry in the array is 4
 * bytes, containing the start address of the zone.
 */
struct zone {
 unsigned int addr : 20;        /* last 12 bits do not matter */
 unsigned int _unallocated: 11; /* reserved for other uses */
 unsigned int free : 1;         /* boolean */
};
struct zonehdr {
 unsigned int next : 22;        /* last 10 bits do not matter */
 unsigned int count : 10;       /* number of zone structs */
 struct zone zones[];           /* array of zones */
};
