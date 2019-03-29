/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

void my_other_process(void *arg)
{
	puts("[pid 1] Salutations world #1!\n");
	relinquish();
	puts("[pid 1] Salutations world #2!\n");
	relinquish();
	puts("[pid 1] Salutations world #3!\n");
	relinquish();
	puts("[pid 1] Salutations world #4!\n");
	while (1) {};
}

void my_process(void* arg)
{
	puts("[pid 0] Hello world #1!\n");
	relinquish();
	puts("[pid 0] Hello world #2!\n");
	relinquish();
	puts("[pid 0] Hello world #3!\n");
	sys0(1);
	puts("[pid 0] Hello world #4! (after making incorrect syscall)\n");
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

	struct process *first = create_process(my_process, NULL);
	struct process *second = create_process(my_other_process, NULL);
	start_process(first);
}
