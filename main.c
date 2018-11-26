/*
 * Main program
 */
#include "lib.h"

void main(void)
{
	puts("Hello world!\n");

	/* Some sysinfo at startup */
	sysinfo();

	/* Setup physical memory allocation system. */
	init_pages();
	show_pages();

	/* Now we can allocate pages for page descriptors, lets go */
	enable_mmu();

	/* Show pages again to see where the pdt went */
	show_pages();

	puts("MMU enabled and still alive\n");
	sysinfo();
}
