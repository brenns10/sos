#include "kernel.h"

void backtrace(void)
{
	uint32_t *sp, *fp, *stackmax, *stackmin;
	int max = 32;
	get_sp(sp);
	get_fp(fp);
	stackmin = (uint32_t *)(((uint32_t)sp) & 0xFFFFF000);
	stackmax = (uint32_t *)((((uint32_t)sp) & 0xFFFFF000) + 0x1000);
	puts("BACKTRACE:");
	while (max-- && fp >= stackmin && fp < stackmax && fp[0]) {
		printf(" 0x%x", fp[0]);
		fp = (uint32_t *)fp[-1];
	}
	puts("\n");
}
