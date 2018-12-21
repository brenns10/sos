/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

void main(uint32_t phys)
{
	puts("Hello world!\n");

	printf("The physical location of the code was originally 0x%x\n", phys);
	printf("It is now 0x%x\n", &code_start);

	printf("\nInitializing memory...\n");
	mem_init(phys);
}
