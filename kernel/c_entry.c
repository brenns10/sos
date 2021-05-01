#include "cxtk.h"
#include "kernel.h"
#include "mm.h"

void print_context(struct ctx *ctx)
{
	printf("PSR: 0x%0x   SP: 0x%0x   LR: 0x%0x\n", ctx->spsr, ctx->sp, ctx->lr);
	printf(" A1: 0x%0x   A2: 0x%0x   A3: 0x%0x\n", ctx->a1, ctx->a2, ctx->a3);
	printf(" A4: 0x%0x  R12: 0x%0x   V1: 0x%0x\n", ctx->a4, ctx->r12, ctx->v1);
	printf(" V2: 0x%0x   V3: 0x%0x   V4: 0x%0x\n", ctx->v2, ctx->v3, ctx->v4);
	printf(" V5: 0x%0x   V6: 0x%0x   V7: 0x%0x\n", ctx->v5, ctx->v6, ctx->v7);
	printf(" V8: 0x%0x  RET: 0x%0x\n", ctx->v8, ctx->ret);
}

void print_fault(uint32_t fsr, uint32_t far, struct ctx *ctx)
{
	print_context(ctx);
	vmem_diag(far);
	switch (fsr & 0x40F) {
	case 0x5:
		printf("Translation fault (section): 0x%x\n", far);
		break;
	case 0x7:
		printf("Translation fault (page): 0x%x\n", far);
		break;
	case 0x9:
		printf("Domain fault (section): 0x%x, domain=%u\n", far,
		       (fsr >> 4) & 0xF);
		break;
	case 0xB:
		printf("Domain fault (page): 0x%x, domain=%u\n", far,
		       (fsr >> 4) & 0xF);
		break;
	case 0xD:
		printf("Permission fault (section): FAR=0x%x, FSR=0x%x\n", far,
		       fsr);
		break;
	case 0xF:
		printf("Permission fault (page): FAR=0x%x, FSR=0x%x\n", far,
		       fsr);
		break;
	default:
		printf("Some other fault\n");
		break;
	}
	cxtk_report();
	backtrace_ctx(ctx);
	puts("END OF FAULT REPORT\n");
}

void data_abort(struct ctx *ctx)
{
	uint32_t dfsr, dfar;
	get_cpreg(dfsr, c5, 0, c0, 0);
	get_cpreg(dfar, c6, 0, c0, 0);
	printf("ERR: Data Abort! DFSR=%x DFAR=%x\n", dfsr, dfar);
	print_fault(dfsr, dfar, ctx);
	cpu_infinite_loop();
}

void prefetch_abort(struct ctx *ctx)
{
	uint32_t fsr, far;
	get_cpreg(fsr, c5, 0, c0, 1);
	get_cpreg(far, c6, 0, c0, 2);
	printf("ERR: Prefetch Abort! FSR=%x IFAR=%x\n", fsr, far);
	print_fault(fsr, far, ctx);
	cpu_infinite_loop();
}

void irq(struct ctx *ctx)
{
	uint8_t intid = (uint8_t)gic_interrupt_acknowledge();
	cxtk_track_irq(intid, ctx->ret);
	isr_t isr = gic_get_isr(intid);
	/*
	 * You might think that during IRQ mode, interrupts are disabled. But
	 * that's not really true. The GIC is what serializes interrupts... as
	 * soon as you tell the GIC the interrupt is over, it will happily
	 * interrupt you again, while you are still in IRQ mode.
	 *
	 * In our case, this means that the context saved on the IRQ stack is
	 * corrupted, and returning from the second (or Nth) interrupt is
	 * unsafe.
	 *
	 * The best way to avoid this silliness is to just disable interrupts.
	 * Once our handler fully returns (using rfefd), interrupts will be
	 * restored, and any pending interrupts will be triggered immediately
	 * without corruption.
	 */
	interrupt_disable();
	if (isr)
		isr(intid, ctx);
	else
		printf("Unhandled IRQ: ID=%u, not ending\n", intid);
}

void fiq(struct ctx *ctx)
{
	puts("FIQ!\n");
}

void undefined(struct ctx *ctx, uint32_t *pc)
{
	puts("ERR: Undefined instruction!\n");
	print_context(ctx);
	printf("Instruction 0x%x is 0x%x\n", ctx->ret, *(uint32_t *)ctx->ret);
	cxtk_report();
	backtrace_ctx(ctx);
	puts("END OF FAULT REPORT\n");
	cpu_infinite_loop();
}
