/*
 * Routines for dealing with processes.
 */
#include "kernel.h"

struct list_head process_list;
struct process *current;

#define stack_size 4096

struct static_binary {
	void *start;
	void *end;
};

struct static_binary binaries[] = {
	{.start=process_salutations_start, .end=process_salutations_end},
	{.start=process_hello_start, .end=process_hello_end},
	{}
};

/**
 * Create a process.
 *
 * The returned process resides in the current address space, and has a stack of
 * 4096 bytes (minus the space reserved for the process struct itself). You can
 * take this process and either start it using start_process(), or context
 * switch it in later.
 */
struct process *create_process(struct process *p, uint32_t binary)
{
	static uint32_t pid = 1;
	uint32_t size, phys, i, *dst, *src;
	/*
	 * Determine the size of the "process image" rounded to a whole page
	 */
	size = (
		/* subtract start from end */
		(uint32_t) binaries[binary].end
		- (uint32_t) binaries[binary].start
		/* object file doesn't include stack space, so we assume 8 bytes
		 * of alignment and a page of stack */
		+ 0x1000 + 8
	);
	size = ((size >> PAGE_BITS) + 1) << PAGE_BITS;

	/*
	 * Allocate physical memory for the process, and map it to the "process
	 * segment" that we've defined, 0x20000000.
	 */
	phys = alloc_pages(phys_allocator, size, 0);
	mark_alloc(kern_virt_allocator, 0x20000000, size);
	kmem_map_pages(0x20000000, phys, size, PRW_URW);

	/*
	 * Copy the "process image" over.
	 */
	dst = (uint32_t*) 0x20000000;
	src = (uint32_t*) binaries[binary].start;
	for (i = 0; i < size>>2; i++)
		dst[i] = src[i];

	/*
	 * Set up some process variables
	 */
	p->context[PROC_CTX_SPSR] = 0x10;      /* enter user mode */
	p->context[PROC_CTX_RET] = 0x20000000; /* jump to process img */
	p->id = pid++;
	p->size = size;
	p->phys = phys;
	list_insert(&process_list, &p->list);

	return p;
}

void destroy_process(struct process *proc)
{
	printf("[kernel]\t\tdestroy process %u (p=0x%x)\n", proc->id, proc);
	list_remove(&proc->list);
	kmem_free_pages((void*)0x20000000, proc->size);
}

void start_process(struct process *p)
{
	current = p;
	printf("[kernel]\t\tstart process %u\n", p->id);
	start_process_asm(
		(void*)p->context[PROC_CTX_RET]
	);
}

void context_switch(struct process *new_process)
{
	uint32_t *context = (uint32_t *)&stack_end - nelem(current->context);
	uint32_t i;

	printf("[kernel]\t\tswap current process %u for new process %u\n",
			current ? current->id : 0, new_process->id);
	if (new_process == current)
		return;

	/* save current context if the process is alive */
	if (current)
		for (i = 0; i < nelem(current->context); i++)
			current->context[i] = context[i];

	/* load new context */
	for (i = 0; i < nelem(new_process->context); i++) {
		context[i] = new_process->context[i];
	}

	current = new_process;
}

/**
 * Choose and context switch into a different active process. Can't handle 0
 * processes.
 */
void schedule(void)
{
	struct process *iter, *chosen=NULL;

	list_for_each_entry(iter, &process_list, list, struct process) {
		if (iter != current) {
			chosen = iter;
			break;
		}
	}

	if (chosen) {
		/*
		 * A new process is chosen, move it to the end to give other
		 * processes a chance (round robin scheduler).
		 */
		list_remove(&chosen->list);
		list_insert_end(&process_list, &chosen->list);

		context_switch(chosen);
	} else if (current) {
		/*
		 * There are no other options, continue executing this.
		 */
		return;
	} else {
		/*
		 * No remaining processes, do an infinite loop.
		 */
		printf("[kernel]\t\tNo processes remain.\n");
		while (1){};
	}
}
