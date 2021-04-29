/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "blk.h"
#include "board.h"
#include "cxtk.h"
#include "fs.h"
#include "kernel.h"
#include "socket.h"
#include "string.h"

void main(uint32_t);

void pre_mmu(uint32_t phys)
{
	/*
	 * Called immediately upon startup. This is an excellent place to
	 * implement early logic:
	 *
	 *   - isolate cores of multicore system
	 *   - initialize console
	 *   - setup page tables
	 *
	 * Upon return from this function, the MMU is enabled, we jump to the
	 * destination virtual addres of the kernel, and execute main().
	 *
	 * Note that since at this point, the MMU is disabled, we must be VERY
	 * CAREFUL with any code that uses pointers. Variable and functions are
	 * used via their offset (thanks to position independent code), but if
	 * you create a pointer, it seems that the pointer will refer to the
	 * linker address. Given that we link to the virtual address space, this
	 * is bad. So be careful.
	 */

	// Set .bss to 0
	memset((void *)(phys + (uint32_t)&bss_start - (uint32_t)&code_start), 0,
	       data_end - bss_start);
	uart_init();
	board_premmu();
	kmem_init2(phys);
}

void start_ush(void)
{
	struct process *proc;
	proc = create_process(BIN_USH);
	context_switch(proc);
}

void main(uint32_t phys)
{
	/* WARNING: printing functions may not be used until after uart_remap()!
	 * MMU has been enabled and thus UART is no longer mapped.
	 */
	kmem_init2_postmmu();
	uart_remap();
	puts("SOS: started!\n");
	board_init();
	kmalloc_init();
	process_init();
	dtb_init(0x44000000); /* TODO: pass this addr from startup.s */
	gic_init();
	timer_init();
	fs_init(); /* Initialize file slab before uart file is created */
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
