/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

void start_ush(void)
{
	struct process *proc;
	proc = create_process(BIN_USH);
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
	uart_init();
	virtio_init();

	start_ush();
}
