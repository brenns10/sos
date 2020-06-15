#include "kernel.h"

/*
 *  r0 - a1
 *  r1 - a2
 *  r2 - a3
 *  r3 - a4
 *  r4 - v1
 *  r5 - v2
 *  r6 - v3
 *  r7 - v4
 *  r8 - v5
 *  r9 - v6 - sb
 * r10 - v7
 * r11 - v8 - fp
 * r12 - ip
 * r13 - sp
 * r14 - lr
 * r15 - pc
 */

void backtrace_internal(uint32_t *fp)
{
	uint32_t *stackmax, *stackmin;
	int max = 32;
	stackmin = (uint32_t *)(((uint32_t)fp) & 0xFFFFF000);
	stackmax = (uint32_t *)((((uint32_t)fp) & 0xFFFFF000) + 0x1000);
	puts("BACKTRACE:");
	while (max-- && fp >= stackmin && fp < stackmax && fp[0]) {
		printf(" 0x%x", fp[0]);
		fp = (uint32_t *)fp[-1];
	}
	puts("\n");
}

void backtrace(void)
{
	uint32_t *fp;
	get_fp(fp);
	backtrace_internal(fp);
}

void backtrace_ctx(struct ctx *ctx)
{
	if ((ctx->spsr & ARM_MODE_MASK) != ARM_MODE_USER)
		backtrace_internal((uint32_t *)ctx->v8);
	else
		puts("No backtrace for user-mode context\n");
}

void printregs(struct ctx *ctx)
{
	printf("SP: 0x%x\tLR: 0x%x\n", ctx->sp, ctx->lr);
	printf("a1: 0x%x\tr12: 0x%x\n", ctx->a1, ctx->r12);
	printf("a4: 0x%x\ta3: 0x%x\n", ctx->a4, ctx->a3);
	printf("a2: 0x%x\tv8: 0x%x\n", ctx->a2, ctx->v8);
	printf("v7: 0x%x\tv6: 0x%x\n", ctx->v7, ctx->v6);
	printf("v5: 0x%x\tv4: 0x%x\n", ctx->v5, ctx->v4);
	printf("v3: 0x%x\tv2: 0x%x\n", ctx->v3, ctx->v2);
	printf("v1: 0x%x\tret: 0x%x\n", ctx->v1, ctx->ret);
	printf("spsr: 0x%x\n", ctx->spsr);
}
