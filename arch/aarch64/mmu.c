/*
 * aarch64 - mmu.c
 *
 * Implementation of page table management routines for aarch64 architecture.
 * This implementation is specific to a 4KiB memory granule and 48-bit input
 * address length. 1GiB and 2MiB block mappings are allowed as well.
 *
 * Now that I'm generalizing the page table management, it occurs to me I need
 * to decide what the API for memory management is. We need:
 *
 * 1) direct map region which maps all physical memory
 *    -> A way to tell the memory allocator what the region is, and what's
 *       available.
 *    -> Uses minimal number of PTEs
 * 2) a virtual memory region for mapping the kernel code
 *    -> Read-only, execute permissions set.
 *    -> The only API is that we jump the kernel here
 * 3) a virtual memory region for device memory mapping (or vmalloc)
 *    -> Supports only 4KiB mappings
 *    -> kmem_map_periph()
 *
 * Initialization sequence:
 *
 * 1. Running w/ no MMU. Running in physical address space (the lower range).
 * 2. Create a full upper address range for the kernel (0xFFFF...)
 *    a. Populate direct map region, using 1GiB chunks.
 *    b. Map kernel code at constant upper location (use largest table entries)
 * 3. Create a dummy initial page table for the lower kernel range, which simply
 *    maps the kernel code. Has level 0-3 tables.
 * 4. Allow non-C arch code to enable MMU and caches, etc.
 * 5. The lower-level assembly finally branches into the upper address range
 *    kernel mapping location.
 * 6. Follow-up initialization code removes the initial TTBR0 tables.
 */
#include <stdint.h>
#include <stdbool.h>

#include <arch/aarch64_cpreg.h>

#include "kernel.h"
#include "mm.h"
#include "alloc.h"
#include "string.h"

// Bits resolved per level
#define NUM_BITS 9

// PTEs per table at a level
#define NUM_PTES (1 << NUM_BITS)

#define PG_BITS 12
#define PG_SIZE (1 << PG_BITS)
#define PG_MASK (PG_SIZE - 1)

#define L3_SHIFT (PG_BITS)
#define L3_SIZE (1ULL << L3_SHIFT)
#define L3_MASK ((NUM_PTES - 1ULL) << L3_SHIFT)
#define L3_IDX(x) ((unsigned int)(((x) & L3_MASK) >> L3_SHIFT))

#define L2_SHIFT (L3_SHIFT + NUM_BITS)
#define L2_SIZE (1ULL << L2_SHIFT)
#define L2_MASK ((NUM_PTES - 1ULL) << L2_SHIFT)
#define L2_IDX(x) ((unsigned int)(((x) & L2_MASK) >> L2_SHIFT))

#define L1_SHIFT (L2_SHIFT + NUM_BITS)
#define L1_SIZE (1ULL << L1_SHIFT)
#define L1_MASK ((NUM_PTES - 1ULL) << L1_SHIFT)
#define L1_IDX(x) ((unsigned int)(((x) & L1_MASK) >> L1_SHIFT))

#define L0_SHIFT (L1_SHIFT + NUM_BITS)
#define L0_SIZE (1ULL << L0_SHIFT)
#define L0_MASK ((NUM_PTES - 1ULL) << L0_SHIFT)
#define L0_IDX(x) ((unsigned int)(((x) & L0_MASK) >> L0_SHIFT))

#define PTE_TYPE(x) ((x) & 3ULL)
#define PTE_INVALID 0ULL
#define PTE_BLOCK   1ULL
#define PTE_TABLE   3ULL

#define PT3_TYPE(x) ((x) & 3ULL)
#define PT3_INVALID 0ULL
#define PT3_PAGE    3ULL

#define NEXT_TABLE(x, mmu) ((uintptr_t *)(((x) & ADDR_MASK) + (mmu ? arch_direct_map_offset : 0)))

#define ADDR_MASK 0x0000FFFFFFFFF000ULL
#define PT3_ADDR(x) ((x) & ADDR_MASK)

#define AP_RW 0ULL
#define AP_RO (1 << 7)
#define AP_US (1 << 6)
#define AP_SY 0ULL

#define ATTR_NG (1 << 11)
#define ATTR_AF (1 << 10)

// Publicly visible arch-maintained values
void *arch_phys_allocator;
uintptr_t arch_direct_map_offset;
const uintptr_t ARCH_DIRECTMAP = 0xFFFF000000000000;

// Private
uintptr_t *pgtable;
uintptr_t *tmppgt0;


static inline bool aligned(uintptr_t num, unsigned int num_zero)
{
	return !(num & ((1ULL << num_zero) - 1));
}

static inline uintptr_t alloc_page_table(void)
{
	return alloc_pages(arch_phys_allocator, PAGE_SIZE, 12);
}

/**
 * Create a mapping from virt -> phy, spanning len bytes, within page table pgt.
 *
 * This function tries to create a mapping using the largest page table blocks
 * available: 1 GiB, 2 MiB, or 4KiB. To do this, virt and phy should share as
 * much alignment as possible. It's convenient, but not required, for virt and
 * phys to start at a high alignment (e.g. a multiple of 1 GiB), and for len to
 * be a multiple of the highest possible alignment.
 *
 * Only the least significant 48 bits of virt are considered, and virt + phy
 * MUST be at least page aligned. Similarly, len MUST be page aligned. None of
 * these preconditions are checked -- you're on your own here.
 *
 * Leaf entries are created with attributes in attrs (bitwise OR).  New
 * intermediate page tables are allocated as necessary. You cannot overwrite an
 * existing mapping: your new mapping MUST be completely disjoint (adjacent is
 * allowed) from ALL other mappings in the table.
 */
static void map_blocks(uintptr_t *pgt, uintptr_t virt, uintptr_t phy, uintptr_t len, uintptr_t attrs)
{
	uintptr_t *p1, *p2, *p3;

	// Has the MMU been enabled yet? We can tell by the value of the pgt
	// pointer. If it points to an address under the kernel start
	// (0xFFFF...) then we know it is a physical address, and so we're
	// operating with MMU disabled. If it points to an address under the
	bool mmu = (uintptr_t)pgt >= ARCH_DIRECTMAP;

	for (unsigned int idx0 = L0_IDX(virt); idx0 < NUM_PTES && len; idx0++) {
		// 512 GiB blocks are not supported, we must always create (or
		// use an existing) L1 table.
		if (PTE_TYPE(pgt[idx0]) != PTE_TABLE) {
			pgt[idx0] = alloc_page_table() | PTE_TABLE;
			memset(NEXT_TABLE(pgt[idx0], mmu), PAGE_SIZE, (char)0);
		}
		p1 = NEXT_TABLE(pgt[idx0], mmu);

		for (unsigned int idx1 = L1_IDX(virt); idx1 < NUM_PTES && len; idx1++) {
			// See if we can do a 1GiB block: Both virt and phy must
			// be aligned to the 1 GiB size, and the mapping needs
			// to extend at least 1 GiB.
			if (aligned(phy, L1_SHIFT) && aligned(virt, L1_SHIFT) && len >= L1_SIZE) {
				// Overlapping/overwriting mappings is
				// disallowed, so there should be no existing
				// entry. To simplify, don't check!
				p1[idx1] = phy | attrs | PTE_BLOCK;
				phy += L1_SIZE;
				virt += L1_SIZE;
				len -= L1_SIZE;
				continue;
			}
			// We must create an (or use an existing) L2 table.
			if (PTE_TYPE(p1[idx1]) != PTE_TABLE) {
				p1[idx1] = alloc_page_table() | PTE_TABLE;
				memset(NEXT_TABLE(p1[idx1], mmu), PAGE_SIZE, (char)0);
			}
			p2 = NEXT_TABLE(p1[idx1], mmu);

			for (unsigned int idx2 = L2_IDX(virt); idx2 < NUM_PTES && len; idx2++) {
				// See if we can do a 2MiB block: Both virt and phy must
				// be aligned to the 2 MiB size, and the mapping needs
				// to extend at least 2 MiB.
				if (aligned(phy, L2_SHIFT) && aligned(virt, L2_SHIFT) && len >= L2_SIZE) {
					// Overlapping / overwriting mappings is
					// disallowed, so there should be no
					// existing entry. Again, don't check.
					p2[idx2] = phy | attrs | PTE_BLOCK;
					phy += L2_SIZE;
					virt += L2_SIZE;
					len -= L2_SIZE;
					continue;
				}

				// We must create an (or use an existing) L3 table.
				if (PTE_TYPE(p2[idx2] != PTE_TABLE)) {
					p2[idx2] = alloc_page_table() | PTE_TABLE;
					memset(NEXT_TABLE(p2[idx2], mmu), PAGE_SIZE, (char)0);
				}
				p3 = NEXT_TABLE(p2[idx2], mmu);

				for (unsigned int idx3 = L3_IDX(virt); idx3 < NUM_PTES && len; idx3++) {
					p3[idx3] = phy | attrs | PT3_PAGE;
					phy += L3_SIZE;
					virt += L3_SIZE;
					len -= L3_SIZE;
				}
			}
		}
	}
}

void show_pgt(uintptr_t *pgt, uintptr_t virt, uintptr_t len)
{
	uintptr_t *p1, *p2, *p3;
	bool mmu = (uintptr_t)pgt >= ARCH_DIRECTMAP;
	printf("L0 PGT @ 0x%0X\n", pgt);
	for (unsigned int idx0 = L0_IDX(virt); idx0 < NUM_PTES && len; idx0++) {
		if (PTE_TYPE(pgt[idx0]) != PTE_TABLE) {
			virt += L0_SIZE;
			len -= L0_SIZE;
			continue;
		}
		p1 = NEXT_TABLE(pgt[idx0], mmu);
		printf("  L1 PGT @ 0x%0X\n", p1);

		for (unsigned int idx1 = L1_IDX(virt); idx1 < NUM_PTES && len; idx1++) {
			if (PTE_TYPE(p1[idx1]) == PTE_BLOCK)
				printf("  0x%X: 0x%0X (1 GiB Block)\n",
				       virt, PT3_ADDR(p1[idx1]));
			if (PTE_TYPE(p1[idx1]) != PTE_TABLE) {
				virt += L1_SIZE;
				len -= L1_SIZE;
				continue;
			}
			p2 = NEXT_TABLE(p1[idx1], mmu);
			printf("    L2 PGT @ 0x%0X\n", p2);

			for (unsigned int idx2 = L2_IDX(virt); idx2 < NUM_PTES && len; idx2++) {
				if (PTE_TYPE(p2[idx2]) == PTE_BLOCK)
					printf("    0x%X: 0x%0X (2 MiB Block)\n",
					       virt, PT3_ADDR(p2[idx2]));
				if (PTE_TYPE(p2[idx2]) != PTE_TABLE) {
					virt += L2_SIZE;
					len -= L2_SIZE;
					continue;
				}
				p3 = NEXT_TABLE(p2[idx2], mmu);
				printf("      L3 PGT @ 0x%0X\n", p3);

				for (unsigned int idx3 = L3_IDX(virt); idx3 < NUM_PTES && len; idx3++) {
					if (PTE_TYPE(p3[idx3]) == PT3_PAGE)
						printf("        0x%0X: 0x%0X (Page)\n",
						       virt, PT3_ADDR(p3[idx3]));
					virt += L3_SIZE;
					len -= L3_SIZE;
				}
			}
		}
	}
}

void arch_premmu(uintptr_t phy_start, uintptr_t memsize, uintptr_t phy_code_start)
{
	arch_direct_map_offset = ARCH_DIRECTMAP - phy_start;
	arch_phys_allocator = (void *)phy_code_start + ((void *)&unused_start - (void *) code_start);
	init_page_allocator(arch_phys_allocator, phy_start, phy_start + memsize);

	// Mark the code, data, and allocator itself as allocated.
	mark_alloc(arch_phys_allocator, phy_code_start,
	           (uintptr_t)&unused_start - (uintptr_t)&code_start + PAGE_SIZE);

	// Create an empty kernel page table
	pgtable = (void *)alloc_pages(arch_phys_allocator, PAGE_SIZE, 12);
	memset(pgtable, PAGE_SIZE, (char)0);

	// Map all of physical memory at the beginning.
	map_blocks(pgtable, ARCH_DIRECTMAP, phy_start, memsize,
	           AP_RW | AP_SY | ATTR_AF);
	_msr(TTBR1_EL1, pgtable);
	show_pgt(pgtable, ARCH_DIRECTMAP, 0x0001000000000000ULL);

	// Create a temporary TTBR0 table. Normally these are for addresses
	// starting with 0x0000, i.e. userspace. However, at boot we are running
	// on physical addresses which must fall within this address range. We
	// need to setup an identity mapping of our code in order to enable the
	// MMU. TTBR0 is both the correct translation table, and conveniently
	// unused right now. So we can setup our temporary table, use it for the
	// identity mapping, and clean it up once we jump into the kernel
	// address space.
	tmppgt0 = (void *)alloc_pages(arch_phys_allocator, PAGE_SIZE, 12);
	memset(tmppgt0, PAGE_SIZE, (char)0);
	map_blocks(tmppgt0, phy_code_start, phy_code_start,
	           (uintptr_t)&unused_start - (uintptr_t)&code_start + PAGE_SIZE,
	           AP_RW | AP_SY | ATTR_AF);
	_msr(TTBR0_EL1, tmppgt0);
	show_pgt(tmppgt0, 0ULL, 0x0001000000000000ULL);

	uint64_t x = 0x980000000;
	_msr(TCR_EL1, x);
}

static void free_pgt(uintptr_t *pgt)
{
	for (unsigned int idx0 = 0; idx0 < NUM_PTES; idx0++) {
		if (PTE_TYPE(pgt[idx0]) != PTE_TABLE)
			continue;
		uintptr_t *p1 = NEXT_TABLE(pgt[idx0], true);
		for (unsigned int idx1 = 0; idx1 < NUM_PTES; idx1++) {
			if (PTE_TYPE(p1[idx1]) != PTE_TABLE)
				continue;
			uintptr_t *p2 = NEXT_TABLE(p1[idx1], true);
			for (unsigned int idx2 = 0; idx2 < NUM_PTES; idx2++) {
				if (PTE_TYPE(p2[idx2]) != PTE_TABLE)
					continue;
				free_pages(arch_phys_allocator, (uintptr_t)NEXT_TABLE(p2[idx2], true), 1);
			}
			free_pages(arch_phys_allocator, (uintptr_t)p2, 1);
		}
		free_pages(arch_phys_allocator, (uintptr_t)p1, 1);
	}
	free_pages(arch_phys_allocator, (uintptr_t)pgt, 1);
}

void arch_postmmu(void)
{
	arch_phys_allocator = (void *)((uintptr_t)arch_phys_allocator + arch_direct_map_offset);
	pgtable = (void *)((uintptr_t)pgtable + arch_direct_map_offset);
	tmppgt0 = (void *)((uintptr_t)tmppgt0 + arch_direct_map_offset);
	free_pgt(tmppgt0);
}

void *arch_devmap(uintptr_t virt_addr, uintptr_t phys_addr, uintptr_t size)
{
	map_blocks(pgtable, virt_addr, phys_addr, size, AP_RW | AP_SY | ATTR_AF);
}
