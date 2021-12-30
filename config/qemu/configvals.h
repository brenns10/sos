#include <arch/config.h>
#define CONFIG_BOARD     BOARD_QEMU
#define CONFIG_UART_BASE 0x09000000
#if defined(ARCH_AARCH64)
#define CONFIG_LINK_ADDR 0xFFFF000000080000
#define CONFIG_LINK_ADDR_PREMMU 0x40080000
#elif defined(ARCH_ARM)
#define CONFIG_LINK_ADDR 0x80010000
#define CONFIG_LINK_ADDR_PREMMU 0x40010000
#else
#error "Unsupported architecture for qemu config"
#endif

#define CONFIG_GIC_IF_BASE   0x08010000
#define CONFIG_GIC_DIST_BASE 0x08000000

#define CONFIG_UART_INTID 33

#define timer_tick timer_tick_fallback
