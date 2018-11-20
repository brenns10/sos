/*
 * Main program
 */
#include "lib.h"

void main(void)
{
	puts("Hello world! System info below:\n");

	sysinfo();

	printf("Gonna try to turn on the MMU...\n");
	enable_mmu();

	puts("MMU enabled and still alive\n");
	sysinfo();
}
