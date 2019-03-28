/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

struct list_head process_list;


struct process *create_process(process_start_t startup, void *arg)
{
	const uint32_t stack_size = 4096;
	void *buffer = get_mapped_pages(stack_size, 0);
	/* place the process struct at the lowest address */
	struct process *p = (struct process *) buffer;
	p->sp = buffer + stack_size;
	p->startup = startup;
	p->arg = arg;
	list_insert(&process_list, &p->list);
	return p;
}

void start_process(struct process *p)
{
	start_process_asm(p->arg, p->startup, p->sp);
}

void my_process(void* arg)
{
	printf("I'm a process, and now I'll syscall.\n");
	relinquish();
	printf("I've returned from the syscall.\n");
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
