#include "arm-mailbox.h"
#include "config.h"
#include "kernel.h"
#include "ksh.h"
#include "rpi-gpio.h"

#if CONFIG_BOARD == BOARD_RPI4B

void board_premmu(void)
{
	/*
	 * Set the UART pins to function as UART.
	 */
	set_gpio_function(14, 4);
	set_gpio_function(15, 4);

	/*
	 * For visual indicators, turn off both leds
	 */
	led_act_off();
	led_pwr_off();
}

uint32_t board_memory_start(void)
{
	return 0;
}

uint32_t board_memory_size(void)
{
	return 0x40000000;
}

void board_init(void)
{
	uint32_t reg;

	/*
	 * Invalidate TLB - this is just superstition on my part after enabling
	 * the caches. It's all in the hopes of getting the strex instructior to
	 * work, but so far nothing has worked.
	 */
	tlbiall();

	gpio_remap();
	mbox_remap();

	led_act_on();
	uart_set_echo(true);

	/*
	 * Enable caches.
	 *
	 * Do this AFTER the remapping of gpio and mbox. Since I have not
	 * properly implemented cache semantics for page table updates, any
	 * remapping done once caches are enabled will go horribly wrong.
	 */
	get_cpreg(reg, c1, 0, c0, 0);
	reg |= (1 << 2);  // cache
	reg |= (1 << 12); // icache
	set_cpreg(reg, c1, 0, c0, 0);
	/*
	 * Memory barrier was found to be required (for some reason?) between
	 * enabling caches, and using synchronization primitives.
	 */
	mb();

	ksh(KSH_SPIN);
}

#endif
