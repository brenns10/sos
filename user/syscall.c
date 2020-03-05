/*
 * syscall.c: code related to system calls
 */
#include "syscall.h"

void puts(char *string)
{
	display(string);
}

void putc(char val)
{
	char str[2];
	str[0] = val;
	str[1] = '\0';
	display(str);
}

int getchar(void)
{
	int retval;
	__asm__ __volatile__ (
		"svc #3\n"
		"mov %[rv], a1"
		: /* output operands */ [rv] "=r" (retval)
		: /* input operands */
		: /* clobbers */ "a1", "a2", "a3", "a4"
	);
	return retval;
}

int runproc(char *imagename)
{
	int retval;
	__asm__ __volatile__ (
		"svc #4\n"
		"mov %[rv], a1"
		: /* output operands */ [rv] "=r" (retval)
		: /* input operands */
		: /* clobbers */ "a1", "a2", "a3", "a4"
	);
	return retval;
}

int getpid(void)
{
	int retval;
	__asm__ __volatile__ (
		"svc #5\n"
		"mov %[rv], a1"
		: /* output operands */ [rv] "=r" (retval)
		: /* input operands */
		: /* clobbers */ "a1", "a2", "a3", "a4"
	);
	return retval;
}
