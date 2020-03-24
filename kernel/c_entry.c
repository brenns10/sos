#include "kernel.h"

void print_fault(uint32_t fsr, uint32_t far)
{
	uint32_t mode;
	get_spsr(mode);
	printf("Fault occurred with PSR: 0x%x\n", mode);
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
}

void data_abort(uint32_t lr)
{
	uint32_t dfsr, dfar;
	get_cpreg(dfsr, c5, 0, c0, 0);
	get_cpreg(dfar, c6, 0, c0, 0);
	printf("Uh-oh... data abort! DFSR=%x DFAR=%x LR=%x\n", dfsr, dfar, lr);
	print_fault(dfsr, dfar);
}

void prefetch_abort(uint32_t lr)
{
	uint32_t fsr, far;
	get_cpreg(fsr, c5, 0, c0, 1);
	get_cpreg(far, c6, 0, c0, 2);
	printf("Uh-oh... prefetch abort! FSR=%x IFAR=%x LR=%x\n", fsr, far, lr);
	print_fault(fsr, far);
}

void irq(void)
{
	uint8_t intid = (uint8_t)gic_interrupt_acknowledge();
	isr_t isr = gic_get_isr(intid);
	if (isr)
		isr(intid);
	else
		printf("Unhandled IRQ: ID=%u, not ending\n", intid);
}

void fiq(void) { puts("FIQ!\n"); }
