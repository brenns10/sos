/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

void my_process(void* arg)
{
	puts("I'm a process, and now I'll syscall.\n");
	relinquish();
	puts("I've returned from the syscall.\n");
	sys0(SYS_RELINQUISH);
	puts("Back again!\n");
	sys0(1);
	puts("oops that was not the right syscall\n");
	while (1) {};
}

void main(uint32_t phys)
{
	puts("Hello world!\n");
	INIT_LIST_HEAD(process_list);

	if (VERBOSE) {
		printf("The physical location of the code was originally 0x%x\n", phys);
		printf("It is now 0x%x\n", &code_start);
	}

	printf("Initializing memory...\n");
	mem_init(phys, VERBOSE);

	printf("Done! main is %x\n", main);
	printf("Done! start_process_asm is %x\n", start_process_asm);

	struct process *p = create_process(my_process, NULL);
	start_process(p);
}
