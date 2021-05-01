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

/*
 * Clear the entire L1 data cache
 *
 * NOTE: doing this in C means that it accesses stack variables. Which is weird
 * because that's memory. Ideally you could do this all in assembly with no
 * memory access (beyond instructions). But this is good enough, especially
 * since if you do it before the cache is enabled, there's no conflict.
 */
void data_cache_clear_all(void)
{
	uint32_t ccsidr, set, way, setshift, wayshift, val;

	/* show me data cache */
	set_csselr(0);

	/* Ensure cache selection is visible */
	mb();

	ccsidr = get_ccsidr();

	way = CCSIDR_last_way(ccsidr);
	set = CCSIDR_last_set(ccsidr);
	setshift = CCSIDR_line_size(ccsidr) + 4 + 4;
	clz(wayshift, way);

	do {
		set = CCSIDR_last_set(ccsidr);
		do {
			val = (way << wayshift) | (set << setshift);
			DCISW(val);
			set--;
		} while (set > 0);
		way--;
	} while (way > 0);

	/* "Commit" the cache invalidations */
	mb();
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

	ICIALLU();
	data_cache_clear_all();

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
	 * Memory barrier (dsb) "commits" all these changes to the cache, i.e.
	 * waits for the cache enablement.
	 */
	mb();
	/*
	 * Ensure all instructions after this are loaded from the newly enabled
	 * caches.
	 */
	isb();

	//ksh(KSH_SPIN);
}

#endif
