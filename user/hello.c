/*
 * hello.c: another simple process
 */
#include <stdint.h>

#include "syscall.h"
#include "format.h"

int main()
{
	uint32_t i;

	for (i = 0; i < 8; i++) {
		printf("Hello world, via system call, #%u\n", i);
	}

	return 0;
}
