#include <arch/config.h>

#define CONFIG_BOARD     BOARD_RPI4B
#define CONFIG_UART_BASE 0xFE201000
#define CONFIG_LINK_ADDR 0x80008000
#define CONFIG_LINK_ADDR_PREMMU 0x00008000

#define CONFIG_GIC_DIST_BASE 0xff841000
#define CONFIG_GIC_IF_BASE   0xff842000

#if false
// high periph
#define CONFIG_GIC_DIST_BASE 0x4c0041000
#define CONFIG_GIC_IF_BASE   0x4c0042000
#endif

#define CONFIG_UART_INTID 153

#define timer_tick mbox_led_timer_tick
