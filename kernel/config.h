/*
 * config.h: kernel build configuration values
 *
 * This file defines constants necessary for a kernel configuration. It then
 * includes the user-provided configuration (configvals.h), and checks it for
 * correctness.
 */
#pragma once

/*
 * Board definitions. See CONFIG_BOARD.
 */
#define BOARD_QEMU  1
#define BOARD_RPI4B 2
#define _BOARD_MAX  2

/* INCLUDE USER CONFIG. BELOW WE INTRODUCE EACH CONFIG VALUE AND CHECK IT. */
#include "configvals.h"

/*
 * CONFIG_BOARD
 * REQUIRED: set which board we are building for.
 */
//#define CONFIG_BOARD BOARD_VALUE_OF_YOUR_CHOICE
#if !defined(CONFIG_BOARD) || CONFIG_BOARD < 1 || CONFIG_BOARD > _BOARD_MAX
#error "CONFIG_BOARD is not properly set"
#endif

/*
 * CONFIG_UART_BASE
 * REQUIRED: set the physical address of the pl011 UART
 */
#if !defined(CONFIG_UART_BASE)
#error "CONFIG_UART_BASE is required"
#endif
