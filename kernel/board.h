/*
 * board.h: Every declaration in this file must be implemented by a board.
 */
#pragma once
#include <stdint.h>

void board_premmu(void);
void board_init(void);
uint32_t board_memory_start(void);
uint32_t board_memory_size(void);
