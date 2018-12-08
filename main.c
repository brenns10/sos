/*
 * Main program
 */
#include "lib.h"
#include "kernel.h"

void main(void)
{
	puts("Hello world!\n");

	printf("Map 0x%x to 0x%x\n", code_start, lookup_phys(code_start));

	puts("MMU enabled and still alive\n");
	sysinfo();
}
