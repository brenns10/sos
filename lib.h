/**
 * Declarations for library functions.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* assembly function */
extern void puts(char *string);

/* C functions */
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

/* Allocate pages of memory */
void *alloc_pages(uint32_t count, uint32_t align);

/* assemply function, so that we can access page table location */
void enable_mmu(void);

/* C helper for creating page tables, called by enable_mmu */
void init_page_tables(uint32_t *base);

void sysinfo(void);

/* useful macros */

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
  "mcr p15, " #op1 ", %[rd], " #CRn ", " #CRm ", " #op2 \
  : [rd] "=r" (src) \
  : \
  : \
  )
