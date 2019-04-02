/*
 * alloc.h: allocates pages of memory, physical or virtual
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
#define PAGE_BITS 12

/**
 * Create a page allocator managing memory from start to end.
 *
 * allocator: pointer to one page of memory (4096 bytes) for use by allocator
 * start: address of first byte of memory managed by the allocator
 * end: address of the first byte of memory no longer managed by us
 */
void init_page_allocator(void *allocator, uint32_t start, uint32_t end);

/**
 * Print out all allocations, for debugging.
 */
void show_pages(void *allocator);

/**
 * Allocate physical pages.
 * count: how many bytes to allocate
 * align: what byte boundary to align on?
 *   <12: default, 4KB aligned
 *   13: 8KB aligned
 *   14: 16KB aligned, etc
 * return: physical pointer to contiguous pages
 *   NULL if the memory could not be allocated
 */
uint32_t alloc_pages(void *allocator, uint32_t count, uint32_t align);

/**
 * Free physical pages.
 * addr: address of range to free
 * count: number of bytes to free
 */
bool free_pages(void *allocator, uint32_t addr, uint32_t count);

/**
 * Mark a memory region as allocated.
 */
bool mark_alloc(void *allocator, uint32_t start, uint32_t count);
