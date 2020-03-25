/*
 * Routines for dealing with processes.
 */
#include "kernel.h"
#include "slab.h"
#include "string.h"

struct list_head process_list;
struct process *current = NULL;
struct slab *proc_slab;
static uint32_t pid = 1;
struct process *idle_process = NULL;

#define stack_size 4096

struct static_binary {
	void *start;
	void *end;
	char *name;
};

struct static_binary binaries[] = {
	{.start=process_salutations_start, .end=process_salutations_end, .name="salutations"},
	{.start=process_hello_start, .end=process_hello_end, .name="hello"},
	{.start=process_ush_start, .end=process_ush_end, .name="ush"},
};

/**
 * Create a process.
 *
 * The returned process resides in the current address space, and has a stack of
 * 4096 bytes (minus the space reserved for the process struct itself). You can
 * take this process and either start it using start_process(), or context
 * switch it in later.
 */
struct process *create_process(uint32_t binary)
{
	uint32_t size, phys, i, *dst, *src, virt;
	struct process *p = slab_alloc(proc_slab);

	/*
	 * Allocate a kernel stack.
	 */
	p->kstack = (void*)kmem_get_pages(4096, 0) + 4096;

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
	p->context[PROC_CTX_SPSR] = ARM_MODE_USER;
	p->context[PROC_CTX_RET] = 0x40000000; /* jump to process img */
	p->id = pid++;
	p->size = size;
	p->phys = phys;
	list_insert(&process_list, &p->list);
	p->flags.pr_ready = 1;
	p->flags.pr_kernel = 0;
	p->flags.pr_syscall = 0;

	/*umem_print(p, 0x40000000, 0xFFFFFFFF);*/

	return p;
}

/**
 * Create a kernel thread! This thread cannot be started with start_process(),
 * but may be context-switched in.
 */
struct process *create_kthread(void (*func)(void*), void *arg)
{
	struct process *p = slab_alloc(proc_slab);
	p->id = pid++;
	p->size = 0;
	p->phys = 0;
	p->flags.pr_ready = 1;
	p->flags.pr_kernel = 1;
	p->flags.pr_syscall = 0;
	p->kstack = (void*)kmem_get_pages(4096, 0) + 4096;

	/* kthread is in kernel memory space, no user memory region */
	p->vmem_allocator = NULL;
	p->ttbr1 = 0;
	p->first = NULL;
	p->shadow = NULL;

	for (int i = 0; i < nelem(p->context); i++)
		p->context[i] = 0;
	p->context[PROC_CTX_SPSR] = (uint32_t)ARM_MODE_SYS;
	p->context[PROC_CTX_A1] = (uint32_t)arg;
	p->context[PROC_CTX_RET] = (uint32_t)func;
	p->context[PROC_CTX_SP] = (uint32_t)(p->kstack);
	return p;
}

void destroy_current_process()
{
	uint32_t i;
	//printf("[kernel]\t\tdestroy process %u (p=0x%x)\n", proc->id, proc);

	/*
	 * Remove from the global process list
	 */
	list_remove(&current->list);

	if (!current->flags.pr_kernel) {
		/*
		 * Free the process image's physical memory (it's not mapped
		 * anywhere except for the process's virtual address space)
		 */
		free_pages(phys_allocator, current->phys, current->size);

		/*
		 * Free the process's virtual memory allocator.
		 */
		kmem_free_pages(current->vmem_allocator, 0x1000);

		/*
		 * Find any second-level page tables, and free them too!
		 */
		for (i = 0; i < 0x1000; i++)
			if (current->shadow[i])
				kmem_free_pages(current->shadow[i], 0x1000);

		/*
		 * Free the first-level table + shadow table
		 */
		kmem_free_pages(current->first, 0x8000);
	} else {
	}

	/*
	 * Free the kernel stack, which we stored the other end of for
	 * our full-descending implementation.
	 *
	 * This is tricky because we're currently using that stack! This here is
	 * a bit of a fudge, but we can simply reset the stack pointer to the
	 * initial kernel stack to enable our final call into schedule.
	 */
	asm volatile("ldr sp, =stack_end" ::: "sp");
	kmem_free_pages((void*)current->kstack - 4096, 4096);
	slab_free(current);

	/*
	 * Mark current AS null for schedule(), to inform it that we can't
	 * continue running this even if there are no other options.
	 */
	current = NULL;
	schedule();
}

/* should be called only once */
void start_process(struct process *p)
{
	/* Set the current ASID */
	set_cpreg(p->id, c13, 0, c0, 1);

	/* Set TTBR1 to user tables! */
	set_cpreg(p->ttbr1, c2, 0, c0, 1);

	current = p;
	//printf("[kernel]\t\tstart process %u (ttbr1=0x%x)\n", p->id, p->ttbr1);
	start_process_asm(
		(void*)p->context[PROC_CTX_RET]
	);
}

void context_switch(struct process *new_process)
{
	uint32_t i, use_retval, mode;

	/*printf("[kernel]\t\tswap current process %u for new process %u\n",
			current ? current->id : 0, new_process->id);*/

	if (new_process == current)
		goto out;


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

	/*
	 * In case that a process had been waiting for a system call and now can
	 * return, we can pretend to return from a syscall. Just be sure to mark
	 * that the process is no longer waiting for a syscall.
	 */
out:
	return_from_exception(0, 0, &current->context);
}

/**
 * Choose and context switch into a different active process. Does not return.
 */
void schedule(void)
{
	struct process *iter, *chosen=NULL;
	int count_seen = 0, count_ready = 0;

	list_for_each_entry(iter, &process_list, list, struct process) {
		count_seen++;
		if (iter->flags.pr_ready) {
			count_ready++;
			if (iter != current) {
				chosen = iter;
				break;
			}
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
	} else if (current && current->flags.pr_ready) {
		/*
		 * There are no other options, but the current process still
		 * exists and is still runnable. Just keep running it.
		 */
		context_switch(current);
	} else {
		/*
		 * At this point, either there is no process available at all,
		 * or no process is ready. We'll use the IDLE process, which is
		 * always marked as not ready, but in reality we can always
		 * idle a bit.
		 */
		static bool warned = false;
		if (count_seen == 0 && !warned) {
			puts("[kernel] WARNING: no more processes remain\n");
			warned = true;
			/* Create a kthread to run ksh. Next time we schedule
			 * (on the next timer tick), it should get chosen and
			 * we'll have our backup. */
			chosen = create_kthread(ksh, NULL);
			list_insert(&process_list, &chosen->list);
		}
		context_switch(idle_process);
	}
}

int32_t process_image_lookup(char *name)
{
	int32_t i;
	for (i = 0; i < nelem(binaries); i++)
		if (strcmp(binaries[i].name, name) == 0)
			return i;
	return -1;
}

int cmd_mkproc(int argc, char **argv)
{
	struct process *newproc;
	int img;
	if (argc != 2) {
		puts("usage: mkproc BINNAME");
		return 1;
	}

	img = process_image_lookup(argv[1]);

	if (img == -1) {
		printf("unknown binary \"%s\"\n", argv[1]);
		return 2;
	}
	newproc = create_process(img);
	printf("created process with pid=%u\n", newproc->id);
	return 0;
}

int cmd_lsproc(int argc, char **argv)
{
	struct process *p;
	list_for_each_entry(p, &process_list, list, struct process) {
		printf("%u\n", p->id);
	}
	return 0;
}

int cmd_execproc(int argc, char **argv)
{
	unsigned int pid;
	struct process *p;
	if (argc != 2) {
		puts("usage: execproc PID\n");
		return 1;
	}

	pid = atoi(argv[1]);

	printf("starting process execution with pid=%u\n", pid);
	list_for_each_entry(p, &process_list, list, struct process) {
		if (p->id == pid) {
			break;
		}
	}

	if (p->id != pid) {
		printf("pid %u not found\n", pid);
		return 2;
	}

	start_process(p);
	return 0; /* never happens :O */
}

static void idle(void *arg)
{
	while (1) {
		asm("wfi");
	}
}

/*
 * Initialization for processes.
 */
void process_init(void)
{
	INIT_LIST_HEAD(process_list);
	proc_slab = slab_new(sizeof(struct process), kmem_get_page, kmem_free_page);
	idle_process = create_kthread(idle, NULL);
	idle_process->flags.pr_ready = 0; /* idle process is never ready */
}
