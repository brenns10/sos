/*
 * System call implementations.
 *
 * These aren't declared in a header since they are only linked against by
 * entry.s. They shouldn't be called by external code anyway.
 */
#include "kernel.h"

void sys_relinquish(void)
{
	puts("[kernel] tRelinquish()\n");
	block(current->context);
}

void sys_display(char *buffer)
{
	puts(buffer);
}

void sys_exit(uint32_t code)
{
	printf("[kernel] Process %u exited with code %u.\n", current->id,
	       code);
	destroy_current_process();
	/* never returns */
}

int sys_getchar(void)
{
	return getc_blocking();
}

int sys_runproc(char *imagename)
{
	int32_t img = process_image_lookup(imagename);
	if (img < 0)
		return -1;
	create_process(img);
	return 0;
}

int sys_getpid(void)
{
	return current->id;
}

void sys_unknown(uint32_t svc_num)
{
	printf("ERROR: unknown syscall number %u!\n", svc_num);
}
