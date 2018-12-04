/*
 * Main program
 */
#include "lib.h"

void main(void)
{
	puts("Hello world!\n");

	/* Some sysinfo at startup */
	sysinfo();

	/* Now we can allocate pages for page descriptors, lets go */
	enable_mmu();

	puts("MMU enabled and still alive\n");
	sysinfo();
}
