/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

void main(uint32_t phys)
{
	kmem_init(phys, VERBOSE);
	process_init();
	dtb_init(0x44000000); /* TODO: pass this addr from startup.s */

	ksh();
}
