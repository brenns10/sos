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

/*
 * Where is the user/kernel split? I say this is a "config" value but really
 * it's here more for documentation. Ideally it would be configurable, and in
 * the future maybe it can be, but right now modifying this alone will not
 * result in a different user/kernel split.
 */
#define CONFIG_KERNEL_START 0x80000000

/*
 * How many megabytes at the top of the kernel address space should be reserved
 * for vmalloc?
 */
#define CONFIG_VMALLOC_MBS 8
