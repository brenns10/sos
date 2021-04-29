/*
 * Routines for dealing with processes.
 */
#include "cxtk.h"
#include "kernel.h"
#include "ksh.h"
#include "slab.h"
#include "socket.h"
#include "string.h"
#include "wait.h"

struct list_head process_list;
struct process *current = NULL;
struct slab *proc_slab;
static uint32_t pid = 1;
struct process *idle_process = NULL;

bool preempt_enabled = true;
const char nopreempt_begin;
const char nopreempt_end;

#define stack_size 4096

struct static_binary {
	void *start;
	void *end;
	char *name;
};

struct static_binary binaries[] = {
	{ .start = process_salutations_start,
	  .end = process_salutations_end,
	  .name = "salutations" },
	{ .start = process_hello_start,
	  .end = process_hello_end,
	  .name = "hello" },
	{ .start = process_ush_start, .end = process_ush_end, .name = "ush" },
};

bool timer_can_reschedule(struct ctx *ctx)
{
	uint32_t cpsr;
	get_cpsr(cpsr);

	/* Can only reschedule if we are in a timer interrupt */
	if ((cpsr & ARM_MODE_MASK) != ARM_MODE_IRQ) {
		return false;
	}

	/* Some functions are in the .nopreempt section, and should never be
	 * preempted */
	if (ctx->ret >= (uint32_t)&nopreempt_begin &&
	    ctx->ret < (uint32_t)&nopreempt_end) {
		return false;
	}

	return preempt_enabled;
}

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
	uint32_t size, i, *dst, *src;
	struct process *p = slab_alloc(proc_slab);

	/*
	 * Allocate a kernel stack.
	 */
	p->kstack = (void *)kmem_get_pages(4096, 0) + 4096;

	/*
	 * Determine the size of the "process image" rounded to a whole page
	 */
	size = (
	        /* subtract start from end */
	        (uint32_t)binaries[binary].end -
	        (uint32_t)binaries[binary].start
	        /* object file doesn't include stack space, so we assume 8 bytes
	         * of alignment and a page of stack */
	        + 0x1000 + 8);
	size = ((size >> PAGE_BITS) + 1) << PAGE_BITS;

	/*
	 * Create an allocator for the user virtual memory space
	 */
	p->vmem_allocator = kmem_get_pages(0x1000, 0);
	// TODO use constants for user address space
	init_page_allocator(p->vmem_allocator, 0x00001000, 0x7FFFFFFF);

	/*
	 * Allocate the first-level table and a shadow table (for virtual
	 * addresses of second-level tables). Since we require physical address
	 * which is aligned (not virtual), need to do it manually.
	 */
	p->first = kmem_get_pages(0x4000, 14);
	p->ttbr0 = kmem_lookup_phys(p->first);
	for (i = 0; i < 0x2000; i++) /* one day I'll implement memset() */
		p->first[i] = 0;

	/*
	 * Allocate physical memory for the process image, and map it
	 * temporarily into kernel memory.
	 */
	p->image = kmem_get_pages(size, 0);

	/*
	 * Copy the "process image" over.
	 */
	dst = (uint32_t *)p->image;
	src = (uint32_t *)binaries[binary].start;
	for (i = 0; i < (size / 4); i++)
		dst[i] = src[i];

	mark_alloc(p->vmem_allocator, 0x40000000, size);
	umem_map_pages(p, 0x40000000, kmem_lookup_phys(p->image), size, UMEM_DEFAULT);

	/*
	 * Set up some process variables
	 */
	p->context.spsr = ARM_MODE_USER;
	p->context.ret = 0x40000000; /* jump to process img */
	p->id = pid++;
	p->size = size;
	list_insert(&process_list, &p->list);
	p->flags.pr_ready = 1;
	p->flags.pr_kernel = 0;

	INIT_LIST_HEAD(p->sockets);
	p->max_fildes = 0;

	wait_list_init(&p->endlist);

	return p;
}

/**
 * Create a kernel thread! This thread cannot be started with start_process(),
 * but may be context-switched in.
 */
struct process *create_kthread(void (*func)(void *), void *arg)
{
	struct process *p = slab_alloc(proc_slab);
	p->id = pid++;
	p->size = 0;
	p->flags.pr_ready = 1;
	p->flags.pr_kernel = 1;
	p->kstack = (void *)kmem_get_pages(4096, 0) + 4096;

	/* kthread is in kernel memory space, no user memory region */
	p->vmem_allocator = NULL;
	p->ttbr0 = 0;
	p->first = NULL;
	p->shadow = NULL;

	INIT_LIST_HEAD(p->sockets);
	p->max_fildes = 0;

	memset(&p->context, 0, sizeof(struct ctx));
	p->context.spsr = (uint32_t)ARM_MODE_SYS;
	p->context.a1 = (uint32_t)arg;
	p->context.ret = (uint32_t)func;
	p->context.sp = (uint32_t)(p->kstack);

	wait_list_init(&p->endlist);
	return p;
}

void destroy_current_process()
{
	uint32_t i;
	struct socket *sock;
	// printf("[kernel]\t\tdestroy process %u (p=0x%x)\n", proc->id, proc);
	preempt_disable();

	/*
	 * Remove from the global process list
	 */
	list_remove(&current->list);

	if (!current->flags.pr_kernel) {
		/*
		 * Free the process image's physical memory (it's not mapped
		 * anywhere except for the process's virtual address space)
		 */
		kmem_free_pages(current->image, current->size);

		/*
		 * Free the process's virtual memory allocator.
		 */
		kmem_free_pages(current->vmem_allocator, 0x1000);

		/*
		 * Find any second-level page tables, and free them too!
		 */

		/*
		 * TODO: Implement kmem_destroy_page_tables or something
		for (i = 0; i < 0x1000; i++)
			if (current->shadow[i])
				kmem_free_pages(current->shadow[i], 0x1000);
		*/

		/*
		 * Free the first-level table + shadow table
		 */
		kmem_free_pages(current->first, 0x8000);
	} else {
	}

	list_for_each_entry(sock, &current->sockets, sockets)
	{
		socket_destroy(sock);
	}

	wait_list_awaken(&current->endlist);
	wait_list_destroy(&current->endlist);

	/*
	 * Free the kernel stack, which we stored the other end of for
	 * our full-descending implementation.
	 *
	 * This is tricky because we're currently using that stack! This here is
	 * a bit of a fudge, but we can simply reset the stack pointer to the
	 * initial kernel stack to enable our final call into schedule.
	 */
	asm volatile("ldr sp, =stack_end" :::);
	kmem_free_pages((void *)current->kstack - 4096, 4096);
	slab_free(proc_slab, current);

	/*
	 * Mark current as null for schedule(), to inform it that we can't
	 * continue running this process even if there are no other options.
	 *
	 * However first we must disable preemption (which will get re-enabled
	 * once the context switch is complete). This is because if we
	 * rescheduled after setting current to NULL, then we would attempt to
	 * store context into a null pointer. If you don't believe it, feel free
	 * to uncomment the WFI instruction and play around.
	 */
	current = NULL;
	/*asm("wfi");*/
	schedule();
}

void __nopreempt context_switch(struct process *new_process)
{
	if (new_process == current) {
		preempt_enable();
		return;
	}

	/* current could be NULL in two cases:
	 * 1. Starting the first process after initialization.
	 * 2. After destroying a process.
	 * In either case, we don't care to store the context, so don't.
	 */
	if (current)
		if (setctx(&current->context))
			return; /* This is where we get scheduled back in */

	/* Set the current ASID */
	set_cpreg(new_process->id, c13, 0, c0, 1);

	/* Set ttbr */
	set_ttbr0(new_process->ttbr0);

	current = new_process;

	/* TODO: It is possible for ASIDs to overlap. Need to check for this and
	 * invalidate caches. */

	/* TODO: Speculative execution could result in weirdness when ASID is
	 * changed and TTBR is changed (non-atomically). See B3.10.4 in ARMv7a
	 * reference for solution. */

	cxtk_track_proc();
	preempt_enable();
	resctx(0, &current->context);
}

struct process *choose_new_process(void)
{
	struct process *iter, *chosen = NULL;
	int count_seen = 0, count_ready = 0;

	list_for_each_entry(iter, &process_list, list)
	{
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

		return chosen;
	} else if (current && current->flags.pr_ready) {
		/*
		 * There are no other options, but the current process still
		 * exists and is still runnable. Just keep running it.
		 */
		return current;
	} else {
		/*
		 * At this point, either there is no process available at all,
		 * or no process is ready. We'll use the IDLE process, which is
		 * always marked as not ready, but in reality we can always
		 * idle a bit.
		 */
		static bool warned = false;
		if (count_seen == 0 && !warned) {
			puts("[kernel] WARNING: no more processes remain, "
			     "dropping into kernel shell\n");
			warned = true;
			chosen = create_kthread(ksh, KSH_BLOCK);
			list_insert(&process_list, &chosen->list);
			return chosen;
		}
		return idle_process;
	}
}

void irq_schedule(struct ctx *ctx)
{
	struct process *new = choose_new_process();

	if (current == new)
		return;

	/* Set the current ASID */
	set_cpreg(new->id, c13, 0, c0, 1);

	/* Set ttbr */
	set_ttbr0(new->ttbr0);

	/* Swap contexts! */
	current->context = *ctx;
	current = new;
	*ctx = current->context;

	cxtk_track_proc();
	/* returning from IRQ is mandatory, swapping ctx will cause us to return
	 * to the new context */
}

/**
 * Choose and context switch into a different active process.
 */
void __nopreempt schedule(void)
{
	struct process *proc;
	preempt_disable();
	cxtk_track_schedule();
	proc = choose_new_process();
	context_switch(proc);
}

int32_t process_image_lookup(char *name)
{
	int32_t i;
	for (i = 0; i < nelem(binaries); i++)
		if (strcmp(binaries[i].name, name) == 0)
			return i;
	return -1;
}

static int cmd_mkproc(int argc, char **argv)
{
	struct process *newproc;
	int img;
	if (argc != 1) {
		puts("usage: proc create BINNAME");
		return 1;
	}

	img = process_image_lookup(argv[0]);

	if (img == -1) {
		printf("unknown binary \"%s\"\n", argv[0]);
		return 2;
	}
	newproc = create_process(img);
	printf("created process with pid=%u\n", newproc->id);
	return 0;
}

static int cmd_lsproc(int argc, char **argv)
{
	struct process *p;
	list_for_each_entry(p, &process_list, list)
	{
		printf("%u\n", p->id);
	}
	return 0;
}

static int cmd_execproc(int argc, char **argv)
{
	unsigned int pid;
	struct process *p;
	if (argc != 1) {
		puts("usage: proc exec PID\n");
		return 1;
	}

	pid = atoi(argv[0]);

	printf("starting process execution with pid=%u\n", pid);
	list_for_each_entry(p, &process_list, list)
	{
		if (p->id == pid) {
			break;
		}
	}

	if (p->id != pid) {
		printf("pid %u not found\n", pid);
		return 2;
	}

	context_switch(p);
	return 0;
}

struct ksh_cmd proc_ksh_cmds[] = {
	KSH_CMD("create", cmd_mkproc, "create new process given binary image"),
	KSH_CMD("ls", cmd_lsproc, "list process IDs"),
	KSH_CMD("exec", cmd_execproc, "run process"),
	{ 0 },
};

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
	proc_slab = slab_new("process", sizeof(struct process), kmem_get_page);
	idle_process = create_kthread(idle, NULL);
	idle_process->flags.pr_ready = 0; /* idle process is never ready */
}
