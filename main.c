/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

#define proc_print(msg) do {\
	pid = current->id; \
	get_sp(sp); \
	printf("[pid=%u sp=0x%x]\t" msg, pid, sp); \
} while (0);

void my_other_process()
{
	uint32_t pid, sp;

	proc_print("Salutations world #1!\n");
	relinquish();

	proc_print("Salutations world #2!\n");
	relinquish();

	proc_print("Salutations world #3!\n");
	relinquish();

	while (1) {};
}

void my_process()
{
	uint32_t pid, sp;

	proc_print("Hello world #1\n");
	relinquish();

	proc_print("Hello world #2\n");
	relinquish();

	proc_print("Hello world #3\n");
	sys0(1);

	proc_print("Hello world #4 (made wrong syscall)\n");

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

	struct process *first = create_process(my_process);
	struct process *second = create_process(my_other_process);
	start_process(first);
}
