#include <stdint.h>

#include "config.h"

#if CONFIG_BOARD == BOARD_QEMU

void board_premmu(void)
{
}

void board_init(void)
{
}

uint32_t board_memory_start(void)
{
        return 0x40000000;
}

uint32_t board_memory_size(void)
{
        return 0x40000000;
}

#endif
