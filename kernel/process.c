/*
 * Routines for dealing with processes.
 */
#include "kernel.h"

struct list_head process_list;
struct process *current = NULL;

#define stack_size 4096

struct static_binary {
	void *start;
	void *end;
	char *name;
};

struct static_binary binaries[] = {
	{.start=process_salutations_start, .end=process_salutations_end, .name="salutations"},
	{.start=process_hello_start, .end=process_hello_end, .name="hello"},
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
	uint32_t size, phys, i, *dst, *src, virt;
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
	 * Create an allocator for the user virtual memory space
	 */
	p->vmem_allocator = kmem_get_pages(0x1000, 0);
	init_page_allocator(p->vmem_allocator, 0x40000000, 0xFFFFFFFF);

	/*
	 * Allocate the first-level table and a shadow table (for virtual
	 * addresses of second-level tables). Since we require physical address
	 * which is aligned (not virtual), need to do it manually.
	 */
	phys = alloc_pages(phys_allocator, 0x8000, 14);
	virt = alloc_pages(kern_virt_allocator, 0x8000, 0);
	kmem_map_pages(virt, phys, 0x8000, PRW_UNA);
	p->first = (uint32_t*)virt;
	p->shadow = (void*)p->first + 0x4000;
	p->ttbr1 = phys;
	for (i = 0; i < 0x2000; i++)  /* one day I'll implement memset() */
		p->first[i] = 0;

	/*
	 * Allocate physical memory for the process image, and map it
	 * temporarily into kernel memory.
	 */
	phys = alloc_pages(phys_allocator, size, 0);
	virt = alloc_pages(kern_virt_allocator, size, 0);
	kmem_map_pages(virt, phys, size, PRW_UNA);

	/*
	 * Copy the "process image" over.
	 */
	dst = (uint32_t*) virt;
	src = (uint32_t*) binaries[binary].start;
	for (i = 0; i < size>>2; i++)
		dst[i] = src[i];

	/*
	 * Remove the temporary mapping from kernel virtual memory, and map the
	 * process image into the actual process address space.
	 */
	kmem_unmap_pages(virt, size);
	free_pages(kern_virt_allocator, virt, size);
	/* Here we invalidate the TLB for the virtual address we just freed.
	 * This fixes a bug where the virtual address gets immediately re-used
	 * and our new mapping is ignored by the TLB. Need to do this more
	 * generally whenever we free virtual addresses TODO. */
	set_cpreg(virt, c8, 0, c7, 3);
	mark_alloc(p->vmem_allocator, 0x40000000, size);
	umem_map_pages(p, 0x40000000, phys, size, PRW_URW);

	/*
	 * Set up some process variables
	 */
	p->context[PROC_CTX_SPSR] = 0x10;      /* enter user mode */
	p->context[PROC_CTX_RET] = 0x40000000; /* jump to process img */
	p->id = pid++;
	p->size = size;
	p->phys = phys;
	list_insert(&process_list, &p->list);


	umem_print(p, 0x40000000, 0xFFFFFFFF);

	return p;
}

void destroy_process(struct process *proc)
{
	uint32_t i;
	printf("[kernel]\t\tdestroy process %u (p=0x%x)\n", proc->id, proc);

	/*
	 * Remove from the global process list
	 */
	list_remove(&proc->list);

	/*
	 * Free the process image's physical memory (it's not mapped anywhere
	 * except for the process's virtual address space)
	 */
	free_pages(phys_allocator, proc->phys, proc->size);

	/*
	 * Free the process's virtual memory allocator.
	 */
	kmem_free_pages(proc->vmem_allocator, 0x1000);

	/*
	 * Find any second-level page tables, and free them too!
	 */
	for (i = 0; i < 0x1000; i++)
		if (proc->shadow[i])
			kmem_free_pages(proc->shadow[i], 0x1000);

	/*
	 * Free the first-level table + shadow table
	 */
	kmem_free_pages(proc->first, 0x8000);
}

/* should be called only once */
void start_process(struct process *p)
{
	/* Set the current ASID */
	set_cpreg(p->id, c13, 0, c0, 1);

	/* Set TTBR1 to user tables! */
	set_cpreg(p->ttbr1, c2, 0, c0, 1);

	current = p;
	printf("[kernel]\t\tstart process %u (ttbr1=0x%x)\n", p->id, p->ttbr1);
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

	/* Set the current ASID */
	set_cpreg(new_process->id, c13, 0, c0, 1);

	/* Set ttbr */
	set_cpreg(new_process->ttbr1, c2, 0, c0, 1);

	current = new_process;

	/* TODO: It is possible for ASIDs to overlap. Need to check for this and
	 * invalidate caches. */

	/* TODO: Speculative execution could result in weirdness when ASID is
	 * changed and TTBR is changed (non-atomically). See B3.10.4 in ARMv7a
	 * reference for solution. */

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
		printf("Here is the physical address space:\n");
		show_pages(phys_allocator);
		printf("Here is the kernel virtual address space:\n");
		show_pages(kern_virt_allocator);
		while (1){};
	}
}

/*
 * Initialization for processes.
 */
void process_init(void)
{
	INIT_LIST_HEAD(process_list);
}
