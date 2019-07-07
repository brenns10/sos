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

/*
 * NOT declared on the stack because that gets clobbered once we start a process
 */
struct process p1, p2;

void main(uint32_t phys)
{
	INIT_LIST_HEAD(process_list);
	kmem_init(phys, VERBOSE);
	ksh();
}
