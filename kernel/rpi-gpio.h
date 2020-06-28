#pragma once

#include <stdint.h>

/**
 * Set a GPIO pin to a function (3-bit value).
 */
extern void set_gpio_function(uint32_t pin, uint32_t mode);

/**
 * Set a GPIO on or off.
 */
extern void set_gpio(uint32_t pin, uint32_t value);

/**
 * Remap the GPIO into virtual memory.
 */
void gpio_remap(void);
