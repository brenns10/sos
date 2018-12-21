/*
 * Memory Management Unit configuration
 *
 * In the qemu virt machine, physical memory begins at 0x40000000 (1GB).
 *
 * We assume that there are 3GB of physical memory, thus we have the entire
 * remaining physical address space. Further, to simplify things, the operating
 * system will have an identity mapping, and 1GB of memory dedicated to it.
 *
 * Applications are given the virtual memory section starting from 0x80000000.
 * The mapping there may not be identity, since the goal is to have multiple
 * applications having isolated memory mappings.
 *
 * MMU is described in ARM Architecture Reference section B4. For now, we'll use
 * first-layer only, which maps the most significant 12 bits into a table, with
 * each entry taking up 4 bytes. The table must be 4 * 2^12 bits long, or 16KB.
 * We can further subdivide into second-layer mappings, but I'm not implementing
 * that yet.
 *
 * The first 2GB (= 2048 entries, = 8192 bytes) therefore are identity mappings,
 * and we'll use the 16MB super-section format to hold them. The remaining 2GB
 * we're going to leave un-mapped, using them some other time.
 *
 * Notes:
 * - Ensure subpages disabled
 * - Ensure S and R are 0 in CP15 register 1
 */
#include <stdint.h>
#include "kernel.h"
#include "lib.h"

/* first level descriptor types */
#define FLD_UNMAPPED 0x00
#define FLD_COARSE   0x01
#define FLD_SECTION  0x02

#define FLD_MASK     0x03

/* second level descriptor types */
#define SLD_UNMAPPED 0x00
#define SLD_LARGE    0x01
#define SLD_SMALL    0x02

#define SLD_MASK     0x03

/* access control for second level */
#define NOT_GLOBAL   (0x1 << 11)
#define PRW_UNA      0x10        /* AP=0b01, APX=0 */
#define PRW_URO      0x20        /* AP=0b10, APX=0 */
#define PRW_URW      0x30        /* AP=0b11, APX=0 */
#define PRO_UNA      0x210       /* AP=0b01, APX=1 */
#define PRO_URO      0x220       /* AP=0b10, APX=1 */
#define EXECUTE_NEVER 0x01

#define top_n_bits(n) (0xFFFFFFFF << (32 - n))
#define bot_n_bits(n) (0xFFFFFFFF >> (32 - n))

/**
 * Initialize first level page descriptor table to an entirely empty mapping,
 * which points to a coarse table located directly after the first level table.
 */
void init_first_level(uint32_t *base)
{
	uint32_t i;
	for (i = 0; i < 4096; i++) {
		base[i] = (uint32_t)&base[4096 + i * 256] | FLD_UNMAPPED;
	}
}


/**
 * Initialize second level page descriptor table to an entirely empty mapping.
 */
void init_second_level(uint32_t *second)
{
	uint32_t i;
	for (i = 0; i < 256; i++)
		second[i] = 0;
}

/**
 * Insert a mapping from a virtual to a physical page.
 * base: page descriptor table base
 * virt: physical virtual address (should be page aligned)
 * phys: physical virtual address (should be page aligned)
 * attrs: access control attributes
 */
void map_page(uint32_t *base, uint32_t virt, uint32_t phys, uint32_t attrs)
{
	uint32_t first_idx = virt >> 20;
	uint32_t second_idx = (virt >> 12) & 0xFF;
	uint32_t *second = (uint32_t*)(base[first_idx] & 0xFFFFFC00);

	if ((base[first_idx] & FLD_MASK) != FLD_COARSE) {
		base[first_idx] &= ~FLD_MASK;
		base[first_idx] |= FLD_COARSE;
		init_second_level(second);
	}

	second[second_idx] = phys & 0xFFFFF000 | attrs | SLD_SMALL;
}

/**
 * Lookup virtual address of virt in page table. Only supports the mechanism we
 * use to setup the MMU.
 */
uint32_t lookup_phys(void *virt_ptr)
{
	uint32_t *base = (uint32_t*)&unused_start;
	uint32_t virt = (uint32_t) virt_ptr;
	uint32_t first_idx = virt >> 20;
	uint32_t *second = (uint32_t*)(base[first_idx] & 0xFFFFFC00);
	uint32_t second_idx = (virt >> 12) & 0xFF;
	return (
		(second[second_idx] & top_n_bits(20)) |
		(virt & bot_n_bits(12))
	);
}

void map_pages(uint32_t *base, uint32_t virt, uint32_t phys, uint32_t len, uint32_t attrs)
{
	uint32_t i;
	for (i = 0; i < len; i += 4096)
		map_page(base, virt + i, phys + i, attrs);
}

void print_second_level(uint32_t virt_base, uint32_t *second)
{
	uint32_t i;
	for (i = 0; i < 256; i++) {
		switch (second[i] & SLD_MASK) {
			case SLD_LARGE:
				printf("\t0x%x: 0x%x (large)\n",
					virt_base + (i << 12),
					second[i] & top_n_bits(16)
				);
				break;
			case 0x3:
			case SLD_SMALL:
				printf("\t0x%x: 0x%x (small), xn=%u, tex=%u, ap=%u, apx=%u\n",
					virt_base + (i << 12),
					second[i] & top_n_bits(20),
					second[i] & 0x1,
					(second[i] >> 6) & 0x7,
					(second[i] >> 4) & 0x3,
					(second[i] & (1 << 9)) ? 1 : 0
				);
				break;
		}
	}
}

void print_first_level(uint32_t *base)
{
	uint32_t i;
	for (i = 0; i < 4096; i++) {
		switch (base[i] & FLD_MASK) {
			case FLD_SECTION:
				printf("0x%x: 0x%x (section), domain=%u\n",
					i << 20,
					base[i] & top_n_bits(10),
					(base[i] >> 5) & 0xF
				);
				break;
			case FLD_COARSE:
				printf("0x%x: 0x%x (second level), domain=%u\n",
					i << 20,
					base[i] & top_n_bits(22),
					(base[i] >> 5) & 0xF
				);
				print_second_level(
					i << 20,
					(uint32_t*) (base[i] & top_n_bits(22))
				);
				break;
			case FLD_UNMAPPED:
			default:
				break;
		}
	}
}

void init_page_tables(uint32_t *base)
{
	uint32_t lo = 0x40010000, hi = 0xC0000000, len;

	init_first_level(base);

	/* Map code above split and to current location */
	len = (uint32_t)&code_end - (uint32_t)&code_start;
	map_pages(base, hi, (uint32_t)&code_start, len, PRO_UNA);
	map_pages(base, lo, (uint32_t)&code_start, len, PRO_UNA);
	hi += len;
	lo += len;

	/* Same with data + stack + page tables */
	len = (uint32_t)&stack_end - (uint32_t)&data_start + 0x00404000;
	map_pages(base, hi, (uint32_t)&data_start, len, PRW_UNA | EXECUTE_NEVER);
	map_pages(base, lo, (uint32_t)&data_start, len, PRW_UNA | EXECUTE_NEVER);
	lo += len;
	hi += len;

	/* Finally include the UART address so we can still print. */
	map_page(base, 0x09000000, 0x09000000, PRW_UNA | EXECUTE_NEVER);
}

void enable_mmu(void)
{
	uint32_t x;

	/* page table is 16KB, aligned on 16KB boundary (2^14) */
	uint32_t *base = (uint32_t*)&unused_start;

	/* write our memory mapping */
	init_page_tables(base);
	/*print_first_level(base);*/

	/* set page table base */
	set_cpreg(base, c2, 0, c0, 0);

	/* set up access control for domain 0 */
	x = 0x1;
	set_cpreg(x, c3, 0, c0, 0);

	/* there is supposed to be some cache invalidation here, not sure how to
	 * do it */

	/* enable mmu */
	get_cpreg(x, c1, 0, c0, 0);
	x |= 0x00800001; /* xp bit: use vmsa6, and mmu enable */
	set_cpreg(x, c1, 0, c0, 0);
}
