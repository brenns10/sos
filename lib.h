/**
 * Declarations for library functions.
 */
#pragma once

#include <stdint.h>

/* assembly function */
extern void puts(char *string);

/* C functions */
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

/* assemply function, so that we can access page table location */
void enable_mmu(void);

/* C helper for creating page tables, called by enable_mmu */
void init_page_tables(uint32_t *base);

void sysinfo(void);
