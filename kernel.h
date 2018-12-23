/*
 * Declarations worth keeping :)
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Linker symbols for virtual memory addresses.
 */
extern uint8_t code_start[];
extern uint8_t code_end[];
extern uint8_t data_start[];
extern uint8_t data_end[];
extern uint8_t stack_start[];
extern uint8_t stack_end[];
extern uint8_t unused_start[];
extern uint8_t dynamic_start[];
/* NB: this is actually a uint32_t array because it points to a table of words */
extern uint32_t first_level_table[];

/*
 * Address of the UART is stored as a variable.
 */
extern uint32_t uart_base;

/*
 * Physical addresses worth remembering. Only valid once mem_init() is done.
 */
extern uint32_t phys_code_start;
extern uint32_t phys_code_end;
extern uint32_t phys_data_start;
extern uint32_t phys_data_end;
extern uint32_t phys_stack_start;
extern uint32_t phys_stack_end;
extern uint32_t phys_first_level_table;
extern uint32_t phys_second_level_table;
extern uint32_t phys_dynamic;

/*
 * Some well-known addresses which we determine at runtime.
 */
extern void *second_level_table;
extern void *phys_allocator;
extern void *kern_virt_allocator;

/*
 * Basic I/O (see uart.s and format.c for details)
 */
void puts(char *string);
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

/*
 * Initialize memory system (see mem.c for details)
 */
void mem_init(uint32_t phys);

/*
 * Look up the physical address corresponding to a virtual address
 */
uint32_t lookup_phys(void *virt_ptr);

/*
 * Page allocator routines (see pages.c for details)
 */
void init_page_allocator(void *allocator, uint32_t start, uint32_t end);
void show_pages(void *allocator);
void *alloc_pages(void *allocator, uint32_t count, uint32_t align);

/*
 * System info debugging command (see sysinfo.c for details)
 */
void sysinfo(void);

/*
 * Set up the stack for all the other modes ahead of time, so we can use them.
 */
void setup_stacks(void *location);

/*
 * C Entry Points
 */
void data_abort(void);

/**
 * Load coprocessor register into dst.
 */
#define get_cpreg(dst, CRn, op1, CRm, op2) \
 __asm__ __volatile__ ( \
  "mrc p15, " #op1 ", %[rd], " #CRn ", " #CRm ", " #op2 \
  : [rd] "=r" (dst) \
  : \
  : \
  )

/**
 * Set coprocessor register from src.
 */
#define set_cpreg(src, CRn, op1, CRm, op2) \
 __asm__ __volatile__ ( \
  "mcr p15, " #op1 ", %[rs], " #CRn ", " #CRm ", " #op2 \
  : [rs] "+r" (src) \
  : \
  : \
  )
