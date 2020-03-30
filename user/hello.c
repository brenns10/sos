/*
 * hello.c: another simple process
 */
#include <stdint.h>

#include "format.h"
#include "syscall.h"

int main()
{
	uint32_t i, j;
	int pid = getpid();

	for (i = 0; i < 8; i++) {
		printf("[pid=%u] Hello world, via system call, #%u\n", pid, i);

		/* do some busy waiting so we see more context switching */
		for (j = 0; j < 300000; j++) {
			asm("nop");
		}
	}

	return 0;
}
