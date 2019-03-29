/*
 * Routines for dealing with processes.
 */
#include "kernel.h"

struct list_head process_list;
struct process *current;

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
	current = p;
	start_process_asm(p->arg, p->startup, p->sp);
}

void context_switch(struct process *new_process)
{
	uint32_t *context = (uint32_t *)&stack_end - nelem(current->context);
	uint32_t i;

	if (new_process == current)
		return;

	/* Save current context to the current process struct. */
	for (i = 0; i < nelem(current->context); i++)
		current->context[i] = context[i];

	/* Load new process context from its struct. */
	for (i = 0; i < nelem(new_process->context); i++)
		context[i] = new_process->context[i];
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

	/* a new process is chosen, move it to the end to give other processes a
	 * chance (round robin scheduler) */
	if (chosen) {
		list_remove(&chosen->list);
		list_insert_end(&process_list, &chosen->list);

		context_switch(chosen);
	}
}
