/*
 * config.h: kernel build configuration values
 *
 * There is currently no system for maintaining or enforcing configuration
 * rules, nor is there a system for getting the makefile to understand the
 * configuration values. We simply copy this header file to kernel/config.h, and
 * edit the CPP declarations.
 */
#define BOARD_QEMU  1
#define BOARD_RPI4B 2

/*
 * REQUIRED: set which board we are building for
 */
#define CONFIG_BOARD BOARD_QEMU
