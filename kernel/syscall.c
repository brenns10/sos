/*
 * System call implementations.
 *
 * These aren't declared in a header since they are only linked against by
 * entry.s. They shouldn't be called by external code anyway.
 */
#include "kernel.h"

void sys_relinquish(void)
{
	puts("[kernel]\t\tRelinquish()\n");
	schedule();
}

void sys_display(char *buffer)
{
	puts(buffer);
}

void sys_exit(uint32_t code)
{
	printf("[kernel]\t\tProcess %u exited with code %u.\n", current->id,
	       code);
	destroy_process(current);
	/*
	 * Mark current AS null for schedule(), to inform it that we can't
	 * continue running this even if there are no other options.
	 */
	current = NULL;
	schedule();
}

int sys_getchar(void)
{
	int result = try_getc();
	if (result < 0) {
		/*
		 * No result is currently available. Tell the UART driver to
		 * mark this process as waiting on a result. When a result is
		 * available, the process will be woken up with it. In the
		 * meantime, schedule a new process.
		 *
		 * NOTE: uart_wait() marks the process as not ready, so that
		 * schedule will not immediately return control to the process
		 * if it's the only one.
		 */
		uart_wait(current);
		schedule();
	} else {
		return result;
	}
}

void sys_unknown(uint32_t svc_num)
{
	printf("ERROR: unknown syscall number %u!\n", svc_num);
}
