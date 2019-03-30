/*
 * Routines for dealing with processes.
 */
#include "kernel.h"

struct list_head process_list;
struct process *current;

/**
 * Create a process.
 *
 * The returned process resides in the current address space, and has a stack of
 * 4096 bytes (minus the space reserved for the process struct itself). You can
 * take this process and either start it using start_process(), or context
 * switch it in later.
 */
struct process *create_process(process_start_t startup)
{
	static uint32_t pid = 1;
	const uint32_t stack_size = 4096;
	void *buffer = get_mapped_pages(stack_size, 0);
	/* place the process struct at the lowest address of the buffer */
	struct process *p = (struct process *) buffer;
	p->context[PROC_CTX_RET] = (uint32_t)startup;
	/* It is very important to setup the SPSR, otherwise the CPU will stay
	 * in SVC mode when we swap to the new process :| */
	p->context[PROC_CTX_SPSR] = 0x10;
	p->context[PROC_CTX_SP] = (uint32_t)buffer + stack_size;
	p->id = pid++;
	list_insert(&process_list, &p->list);
	return p;
}

void destroy_process(struct process *proc)
{
	list_remove(&proc->list);
	/* TODO: free the process data */
}

void start_process(struct process *p)
{
	current = p;
	printf("[kernel]\t\tstart process %u\n", p->id);
	start_process_asm(
		(process_start_t)p->context[PROC_CTX_RET],
		(void*)p->context[PROC_CTX_SP]
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
