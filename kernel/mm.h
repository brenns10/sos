#pragma once

#include <stdint.h>

#include "config.h"
#include <arch/mmu.h>

 int kmem_init(void);

/**
 * Allocate a single page of kernel memory, returning its virtual address.
 */
void *kmem_get_page(void);

/**
 * Allocate some amount of pages with an alignment requirement.
 * @param bytes The number of bytes, must be a multiple of PAGE_SIZE.
 * @param align The alignment requirement (number of least significint bits that
 * are zero)
 */
void *kmem_get_pages(uintptr_t bytes, uintptr_t align);

/**
 * Free a single page.
 * @param ptr The kernel virtual memory address of the page
 */
void kmem_free_page(void *ptr);

/**
 * Free some amount of pages.
 * @param virt_ptr Pointer to the first page
 * @param len Amount of memory to free (multiple of PAGE_SIZE)
 */
void kmem_free_pages(void *virt_ptr, uintptr_t len);

/**
 * Convert a normal kernel virtual address (non-MMIO) to physical.
 */
static inline uintptr_t kvtop(void *ptr)
{
	return ((uintptr_t) ptr - arch_direct_map_offset);
}

/**
 * Given a physical address, return the kernel virtual address. Note that this
 * must be a physical memory address, not a memory mapped peripheral address or
 * something else.
 */
static inline void *kptov(uintptr_t addr)
{
	return (void *) (addr + arch_direct_map_offset);
}

/**
 * Perform a page table walk to determine the physical address of kernel
 * virt_ptr. Note that for the common case (normal kernel memory), you can use
 * kvtop() to directly compute the address. Currently, this is really only
 * useful for memory addresses which were returned by kmem_map_periph().
 */
uintptr_t kmem_lookup_phys(void *virt_ptr);

/**
 * Map an MMIO peripheral's page range into the kernel address space. Takes the
 * physical base address (must be page aligned) and returns the mapped virtual
 * address (also page aligned). The resulting mapping is categorized as device
 * memory, it is accessible only to the kernel, and it is not executable.
 *
 * Unmapping these MMIO peripherals is currently not supported.
 *
 * @param phys_addr Physical address of the peripheral, page aligned
 * @param size Number of pages, in increments of PAGE_SIZE
 * @return Virtual base address of the peripheral
 */
void *kmem_map_periph(uintptr_t phys_addr, uintptr_t size);


struct process;

enum umem_perm {
	UMEM_RW = 0,
	UMEM_RO = 1,
};

/*
 * Below are the process memory management APIs. They are pretty minimal, we
 * should have a full-scale process VMM API which would allow sharing process
 * address spaces, etc. That is all forthcoming. For now, we offer this humble
 * API.
 */

/**
 * Map a physical page into a process's memory address space. The physical
 * address must be page aligned and the size must be in increments of pages.
 * The physical page can only be normal memory -- device memory will not work.
 * @param p Process to insert mapping into
 * @param virt Virtual address to map (page aligned)
 * @param phys Physical address to map (page aligned)
 * @param size Number of bytes to map (increments of PAGE_SIZE)
 * @param perm Permissions of this mapping (RW or RO)
 */
void umem_map_pages(struct process *p, uintptr_t virt, uintptr_t phys, uintptr_t size, enum umem_perm perm);

/**
 * Lookup the mapping for a user virtual address.
 * @param p Process page tables to use
 * @param virt_ptr Virtual address to lookup
 */
uintptr_t umem_lookup_phys(struct process *p, void *virt_ptr);

/**
 * Destroy all memory mappings within the process address space. Note that this
 * simply frees the memory associated with the page tables (e.g. second level
 * pages). It doesn't do anything to free the the mapped memory. The caller
 * should handle that.
 */
void umem_cleanup(struct process *p);

/*
 * Below are declarations of some diagnostic functions.
 */

void vmem_diag(uintptr_t addr);
void mem_print(uintptr_t *base, uintptr_t start, uintptr_t stop);
void kmem_print(uintptr_t start, uintptr_t stop);
