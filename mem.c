/*
 * Memory routines and initialization
 */
#include "kernel.h"

/*
 * MMU Constants
 */

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

/*
 * Well-known physical addresses (computed from linker symbols on startup)
 */
uint32_t phys_code_start;
uint32_t phys_code_end;
uint32_t phys_data_start;
uint32_t phys_data_end;
uint32_t phys_stack_start;
uint32_t phys_stack_end;
uint32_t phys_first_level_table;
uint32_t phys_second_level_table;
uint32_t phys_dynamic;

/*
 * Well-known runtime virtual memory locations.
 */
void *second_level_table;
void *dynamic;

/*
 * Allocators.
 */
void *phys_allocator;
void *kern_virt_allocator;

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
	/*
	 * While normally, you would think that we would determine the second
	 * level table by reading it from the first level descriptor, that won't
	 * work, because it is a physical address. We need to compute a virtual
	 * address for it.
	 */
	uint32_t *second = second_level_table + (first_idx * 1024);

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
	/*
	 * While normally, you would think that we would determine the second
	 * level table by reading it from the first level descriptor, that won't
	 * work, because it is a physical address. We need to compute a virtual
	 * address for it.
	 */
	uint32_t *second = second_level_table + (first_idx * 1024);
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

/**
 * Unmap pages beginning at start, for a total length of len.
 * NOTE: curerntly, start must be aligned to 1 MB, and len must be in increments
 * of 1MB. This will likely change, but right now I'm lazy.
 */
void unmap_pages(uint32_t *base, uint32_t start, uint32_t len)
{
	uint32_t idx;
	idx = start >> 20;
	while (len > 0x00100000) {
		if ((base[idx] & FLD_MASK) != FLD_UNMAPPED) {
			base[idx] &= ~FLD_MASK;
			base[idx] |= FLD_UNMAPPED; /* noop actually */
			init_second_level(
				second_level_table + (idx * 1024));
		}
		len -= 0x00100000;
		idx += 1;
	}
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
					second_level_table + (i * 1024)
				);
				break;
			case FLD_UNMAPPED:
			default:
				break;
		}
	}
}

void mem_init(uint32_t phys)
{
	uint32_t new_uart;
	/*
	 * First, setup some global well-known variables to help ourselves out.
	 */
	phys_code_start = phys + ((void*)code_start - (void*)code_start);
	phys_code_end = phys + ((void*)code_end - (void*)code_start);
	phys_data_start = phys + ((void*)data_start - (void*)code_start);
	phys_data_end = phys + ((void*)data_end - (void*)code_start);
	phys_stack_start = phys + ((void*)stack_start - (void*)code_start);
	phys_stack_end = phys + ((void*)stack_end - (void*)code_start);
	phys_first_level_table = phys + ((void*)first_level_table - (void*)code_start);
	phys_second_level_table = phys_first_level_table + 0x4000;
	phys_dynamic = phys_second_level_table + 0x00400000;
	second_level_table = (void*) first_level_table + 0x4000;
	dynamic = second_level_table + 0x00400000;

	printf("\tphys_code_start = 0x%x\n", phys_code_start);
	printf("\tphys_code_end = 0x%x\n", phys_code_end);
	printf("\tphys_data_start = 0x%x\n", phys_data_start);
	printf("\tphys_data_end = 0x%x\n", phys_data_end);
	printf("\tphys_stack_start = 0x%x\n", phys_stack_start);
	printf("\tphys_stack_end = 0x%x\n", phys_stack_end);
	printf("\tphys_first_level_table = 0x%x\n", phys_first_level_table);
	printf("\tphys_second_level_table = 0x%x\n", phys_second_level_table);
	printf("\tphys_dynamic = 0x%x\n", phys_dynamic);
	printf("\tsecond_level_table = 0x%x\n", second_level_table);
	printf("\tdynamic = 0x%x\n", dynamic);

	/*
	 * Now, let's map some memory and use it to establish two allocators:
	 * (a) Physical memory allocator
	 * (b) Kernel virtual memory allocator
	 */
	phys_allocator = dynamic;
	kern_virt_allocator = dynamic + 0x1000;
	map_pages(first_level_table, (uint32_t) dynamic, phys_dynamic, 0x2000, PRW_UNA | EXECUTE_NEVER);
	init_page_allocator(phys_allocator, phys_dynamic + 0x2000, 0xFFFFFFFF);
	init_page_allocator(kern_virt_allocator, (uint32_t) dynamic + 0x2000, 0xFFFFFFFF);

	printf("We have allocators now!\n");

	/*
	 * Now that we have our allocators, let's allocate some virtual memory
	 * to map the UART at.
	 */
	new_uart = (uint32_t) alloc_pages(kern_virt_allocator, 0x1000, 0);
	printf("Old UART was 0x%x, new will be 0x%x\n", uart_base, new_uart);
	map_page(first_level_table, new_uart, uart_base, PRW_UNA | EXECUTE_NEVER);
	uart_base = new_uart;
	printf("We have made the swap\n");

	show_pages(kern_virt_allocator);

	/*
	 * With the UART moved into kernel space, let's unmap everything before
	 * the kernel-user split.
	 */
	unmap_pages(first_level_table, 0, 0xC0000000);
	printf("We're all in kernel space!\n");

	/*
	 * At this point, we can adjust the flags on the code to be read-only,
	 * and the flags on the data to be execute never. This is safer and
	 * generally a bit more secure.
	 */
	map_pages(first_level_table, (uint32_t) code_start, phys_code_start,
			(uint32_t)(code_end - code_start), PRO_UNA);
	map_pages(first_level_table, (uint32_t) data_start, phys_data_start,
			((uint32_t)first_level_table + 0x00404000 - (uint32_t)data_start),
			PRW_UNA | EXECUTE_NEVER);

	printf("We have adjusted memory permissions!\n");
}
