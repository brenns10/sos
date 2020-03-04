/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

void start_ush(void)
{
	struct process *proc;
	proc = create_process(2);
	start_process(proc);
}


void main(uint32_t phys)
{
	puts("SOS: Startup\n");
	kmem_init(phys, VERBOSE);
	process_init();
	dtb_init(0x44000000); /* TODO: pass this addr from startup.s */
	gic_init();
	timer_init();

	puts("New UART driver\n");
	old_puts("Old UART driver\n");
	puts("New UART driver again\n");
	printf("UART at 0x%x\n", uart_base);
	puts("Hit a char:\n");
	int c = getc();
	printf("You hit char # %u\n", c);
	puts("Again:\n");
	c = old_getc();
	printf("You hit char # %u\n", c);

	start_ush();
}
