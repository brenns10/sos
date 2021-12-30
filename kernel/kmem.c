/*
 * Memory routines and initialization
 */
#include <arch/mmu.h>

#include "kernel.h"
#include "board.h"
#include "util.h"
#include "mm.h"

/*
 * Allocators.
 */
void *kern_virt_allocator;

/**
 * Clean up all second-level tables in a user page table
 */
void umem_cleanup(struct process *p)
{
	arch_umem_free(p->first);
}

/**
 * Public API function, see mm.h
 */
void umem_map_pages(struct process *p, uintptr_t virt, uintptr_t phys,
                    uintptr_t len, enum umem_perm perm)
{
	arch_umem_map(p->first, virt, phys, len, perm);
}

uintptr_t umem_lookup_phys(struct process *p, void *virt_ptr)
{
	return arch_umem_lookup(p->first, virt_ptr);
}

uintptr_t kmem_lookup_phys(void *virt_ptr)
{
	return kvtop(virt_ptr);
}

/**
 * The base page allocation routine for kernel pages. The kernel direct-map
 * address space is managed by kernalloc. Allocate virtual addresses and return
 * them directly. No need to do any mappings.
 */
void *kmem_get_pages(uintptr_t bytes, uintptr_t align)
{
	uintptr_t phys = alloc_pages(arch_phys_allocator, bytes, align);
	return kptov(phys);
}

/**
 * Allocate a single kernel page (a useful helper for the slab allocator)
 */
void *kmem_get_page(void)
{
	return kmem_get_pages(4096, 0);
}

/**
 * Free memory which was allocated via kmem_get_pages().
 */
void kmem_free_pages(void *virt_ptr, uintptr_t len)
{
	free_pages(arch_phys_allocator, kvtop(virt_ptr), len);
}

void kmem_free_page(void *ptr)
{
	kmem_free_pages(ptr, PAGE_SIZE);
}

/**
 * API function - see mm.h
 */
void *kmem_map_periph(uintptr_t phys, uintptr_t len)
{
	uintptr_t virt = alloc_pages(kern_virt_allocator, len, 0);
	arch_devmap(virt, phys, len);
	//mb();
	//isb();
	return (void *)virt;
}

/**
 * Do memory initialization after MMU enabled.
 *
 * This is miscellaneous stuff - clean up the identity mappings, remove the page
 * allocators, and be sure the other processor modes have a stack pointer.
 */
int kmem_init(void)
{
	kern_virt_allocator = kmem_get_page();
	init_page_allocator(kern_virt_allocator, VMALLOC_START, VMALLOC_END);
	return 0;
}
