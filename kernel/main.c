/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "blk.h"
#include "cxtk.h"
#include "kernel.h"
#include "socket.h"

#define VERBOSE false

void start_ush(void)
{
	struct process *proc;
	proc = create_process(BIN_USH);
	context_switch(proc);
}

void main(uint32_t phys)
{
	puts("SOS: Startup\n");
	kmem_init(phys, VERBOSE);
	kmalloc_init();
	process_init();
	dtb_init(0x44000000); /* TODO: pass this addr from startup.s */
	gic_init();
	timer_init();
	uart_init();
	packet_init();
	blk_init();
	virtio_init();
	cxtk_init();

	socket_init();
	udp_init();
	dhcp_kthread_start();

	start_ush();
}
