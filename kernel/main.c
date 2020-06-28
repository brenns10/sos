/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "blk.h"
#include "board.h"
#include "cxtk.h"
#include "kernel.h"
#include "socket.h"

void main(uint32_t);

void pre_mmu(void)
{
	/*
	 * Called immediately upon startup. This is an excellent place to
	 * implement early logic:
	 *   - isolate cores of multicore system
	 *   - initialize console peripheral for early communication
	 *   - setup page tables (soon^TM)
	 *
	 * Upon return from this function, the MMU is enabled, we jump to the
	 * destination virtual addres of the kernel, and execute main().
	 */
	uart_init();
}

void start_ush(void)
{
	struct process *proc;
	proc = create_process(BIN_USH);
	context_switch(proc);
}

void main(uint32_t phys)
{
	puts("SOS: Startup\n");
	kmem_init(phys);
	uart_remap();
	board_init();
	kmalloc_init();
	process_init();
	dtb_init(0x44000000); /* TODO: pass this addr from startup.s */
	gic_init();
	timer_init();
	uart_init_irq();
	packet_init();
	blk_init();
	virtio_init();
	cxtk_init();

	socket_init();
	udp_init();
	dhcp_kthread_start();

	start_ush();
}
