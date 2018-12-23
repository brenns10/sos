#include "kernel.h"

void print_fault(uint32_t fsr, uint32_t far)
{
	switch (fsr & 0x40F) {
	case 0x5:
		printf("Translation fault (section): 0x%x\n", far);
		break;
	case 0x7:
		printf("Translation fault (page): 0x%x\n", far);
		break;
	case 0x9:
		printf("Domain fault (section): 0x%x, domain=%u\n",
				far, (fsr >> 4) & 0xF);
		break;
	case 0xB:
		printf("Domain fault (page): 0x%x, domain=%u\n",
				far, (fsr >> 4) & 0xF);
		break;
	case 0xD:
		printf("Permission fault (section): 0x%x\n", far);
		break;
	case 0xF:
		printf("Permission fault (page): 0x%x\n", far);
		break;
	default:
		printf("Some other fault\n");
		break;
	}
}

void data_abort(void)
{
	uint32_t dfsr, dfar;
	get_cpreg(dfsr, c5, 0, c0, 0);
	get_cpreg(dfar, c6, 0, c0, 0);
	puts("Uh-oh... data abort!\n");
	print_fault(dfsr, dfar);
}

void prefetch_abort(void)
{
	uint32_t fsr, far;
	get_cpreg(fsr, c5, 0, c0, 1);
	get_cpreg(far, c6, 0, c0, 2);
	puts("Uh-oh... prefetch abort!\n");
	print_fault(fsr, far);
}

void swi(void)
{
	puts("Software interrupt!\n");
}

void irq(void)
{
	puts("IRQ!\n");
}

void fiq(void)
{
	puts("FIQ!\n");
}
