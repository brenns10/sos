#include "arm-mailbox.h"
#include "config.h"
#include "kernel.h"
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

void board_init(void)
{
	uint32_t reg;

	/*
	 * Enable caches
	 */
	get_cpreg(reg, c1, 0, c0, 0);
	reg |= (1 << 2);  // cache
	reg |= (1 << 12); // icache
	set_cpreg(reg, c1, 0, c0, 0);

	/*
	 * Invalidate TLB - this is just superstition on my part after enabling
	 * the caches. It's all in the hopes of getting the strex instructior to
	 * work, but so far nothing has worked.
	 */
	tlbiall();

	gpio_remap();
	mbox_remap();

	led_act_on();

	ksh(KSH_SPIN);
}

#endif
