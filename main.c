/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "lib.h"
#include "kernel.h"

void main(void)
{
	puts("Hello world!\n");

	printf("Code start is mapped from 0x%x to 0x%x\n",
			code_start, lookup_phys(code_start));

	sysinfo();
}
