#pragma once

#include <stdint.h>

/*
 * Linker symbols. All of these refer to physical addresses.
 */
extern uint8_t code_start[];
extern uint8_t code_end[];
extern uint8_t data_start[];
extern uint8_t data_end[];
extern uint8_t stack_start[];
extern uint8_t stack_end[];
extern uint8_t unused_start[];
extern uint8_t dynamic_start[];

/*
 * Address of the UART
 */
uint32_t uart_base;
