#include <stdint.h>

#include "kernel.h"
#include "mm.h"

uint32_t gpio_address = 0xFE200000;

void set_gpio_function(uint32_t pin, uint32_t fn)
{
	uint32_t *addr;
	uint32_t word, shift, val;

	if (pin > 53 || fn > 7)
		return; /* invalid */

	addr = (uint32_t *)gpio_address;
	word = pin / 10;
	shift = (pin % 10) * 3;
	val = READ32(addr[word]);
	val &= (~(0x7 << shift)); /* clear bits */
	val |= fn << shift;       /* set new fn*/
	WRITE32(addr[word], val);
}

void set_gpio(uint32_t pin, uint32_t val)
{
	uint32_t *addr;
	uint32_t word, bit, reg;

	if (pin > 53)
		return;

	addr = (uint32_t *)gpio_address;
	word = pin / 32;
	bit = pin % 32;
	reg = READ32(addr[word]);
	if (val) {
		reg |= 1 << bit;
	} else {
		reg &= ~(1 << bit);
	}
	WRITE32(addr[word], reg);
}

void gpio_remap(void)
{
	gpio_address = (uint32_t)kmem_map_periph(gpio_address, 0x1000);
}
