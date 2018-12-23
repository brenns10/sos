/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

void main(uint32_t phys)
{
	puts("Hello world!\n");
	mem_init(phys);
	puts("Memory initialized!\n");
}
