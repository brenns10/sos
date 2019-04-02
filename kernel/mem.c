/*
 * Memory routines and initialization
 */
#include "kernel.h"

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

bool unmap_second(uint32_t *second, uint32_t start, uint32_t len)
{
	uint32_t i;
	uint32_t base = (start >> 12) & 0xFF;
	uint32_t num_descriptors = (len >> 12) & 0xFF;
	for (i = base; i < base + num_descriptors; i++) {
		second[i] = 0;
	}

	/*
	 * For cleanliness, we now check if all second level descriptors are now
	 * unmapped. If so, return true that we can unmap the first level block.
	 */
	for (i = 0; i < 256; i++)
		if (second[i] & SLD_MASK != SLD_UNMAPPED)
			return false;
	return true;
}

/**
 * Unmap pages beginning at start, for a total length of len.
 */
void unmap_pages(uint32_t *base, uint32_t start, uint32_t len)
{
	/*
	 * First, unmap any second level descriptors before the first 1MB
	 * boundary, if there are any.
	 */
	if (start & bot_n_bits(20)) {
		uint32_t first_idx = start >> 20;
		uint32_t to_unmap;
		uint32_t next_mb = (start & top_n_bits(12)) + (1 << 20);
		uint32_t *second = second_level_table + (first_idx * 1024);
		if (next_mb - start < len)
			to_unmap = next_mb - start;
		else
			to_unmap = len;
		if (unmap_second(second, start, to_unmap)) {
			base[first_idx] &= ~FLD_MASK;
			base[first_idx] |= FLD_UNMAPPED;
		}
		len -= to_unmap;
		start = next_mb;
	}

	/*
	 * Next, unmap all 1MB regions, if there are any. We erase their
	 * second-level tables too, for safety, but it's not really necessary,
	 * because map_pages() will erase them when in re-maps a 1MB block.
	 */
	while (len > 0x00100000) {
		uint32_t idx = start >> 20;
		if ((base[idx] & FLD_MASK) != FLD_UNMAPPED) {
			base[idx] &= ~FLD_MASK;
			base[idx] |= FLD_UNMAPPED; /* noop actually */
			init_second_level(
				second_level_table + (idx * 1024));
		}
		len -= 1 << 20;
		start += 1 << 20;
	}

	/*
	 * Finally, unmap any second-level descriptors after the last 1MB block,
	 * if there are any.
	 */
	if (len) {
		uint32_t first_idx = start >> 20;
		uint32_t *second = second_level_table + (first_idx * 1024);
		if (unmap_second(second, start, len)) {
			base[first_idx] &= ~FLD_MASK;
			base[first_idx] |= FLD_UNMAPPED;
		}
	}
}

/**
 * Print out second level table entries.
 * second: pointer to the second level table
 * start: first virtual address to print entries for
 * end: last virtual address to print entries for (may be outside of range)
 *
 * For start and end, we really only care about the middle bits that determine
 * which second level descriptor to use.
 */
void print_second_level(uint32_t *second, uint32_t start, uint32_t end)
{
	uint32_t virt_base = start & top_n_bits(12);
	uint32_t i = (start >> 12) & 0xFF;

	while (start < end && i < 256) {
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
			default:
				printf("\t0x%x: unmapped\n", virt_base + (i<<12));
		}
		i += 1;
		start = virt_base | (i << 12);
	}
}

void print_first_level(uint32_t start, uint32_t stop)
{
	uint32_t *base = first_level_table;
	uint32_t i = start >> 20;
	uint32_t stop_idx = stop >> 20;
	while (i <= stop_idx) {
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
					second_level_table + (i * 1024),
					start, stop
				);
				break;
			case FLD_UNMAPPED:
			default:
				break;
		}
		i += 1;
		start = i << 20;
	}
}

/**
 * Get mapped pages. That is, allocate some physical memory, allocate some
 * virtual memory, and map the virtual to the physical and return it.
 * bytes: count of bytes to allocate (increments of 4096 bytes)
 * align: alignment (only applied to the virtual address allocation)
 */
void *get_mapped_pages(uint32_t bytes, uint32_t align)
{
	void *virt = (void*)alloc_pages(kern_virt_allocator, bytes, align);
	uint32_t phys = alloc_pages(phys_allocator, bytes, 0);
	map_pages(first_level_table, (uint32_t) virt, phys, bytes, PRW_URW | EXECUTE_NEVER);
	return virt;
}

/**
 * Free memory which was allocated via get_mapped_pages. This involves:
 * 1. Determine the physical address, we can do this via a software page table
 *    walk.
 * 2. Free the memory segments from the physical and virtual allocators.
 * 3. Unmap the virtual memory range.
 *
 * virt_ptr: virtual address pointer (must be page aligned)
 * len: length (must be page aligned)
 */
void free_mapped_pages(void *virt_ptr, uint32_t len)
{
	uint32_t phys = lookup_phys(virt_ptr);
	uint32_t virt = (uint32_t) virt_ptr;

	free_pages(phys_allocator, phys, len);
	free_pages(kern_virt_allocator, virt, len);

	unmap_pages(first_level_table, virt, len);
}

void mem_init(uint32_t phys, bool verbose)
{
	uint32_t new_uart, old_uart;
	void *stack;
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

	if (verbose) {
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
	}

	/*
	 * Now, let's map some memory and use it to establish two allocators:
	 * (a) Physical memory allocator
	 * (b) Kernel virtual memory allocator
	 */
	phys_allocator = dynamic;
	kern_virt_allocator = dynamic + 0x1000;
	map_pages(first_level_table, (uint32_t) dynamic, phys_dynamic, 0x2000, PRW_URW | EXECUTE_NEVER);
	init_page_allocator(phys_allocator, phys_dynamic + 0x2000, 0xFFFFFFFF);
	init_page_allocator(kern_virt_allocator, (uint32_t) dynamic + 0x2000, 0xFFFFFFFF);

	if (verbose)
		printf("We have allocators now!\n");

	/*
	 * Now that we have our allocators, let's allocate some virtual memory
	 * to map the UART at.
	 */
	new_uart = alloc_pages(kern_virt_allocator, 0x1000, 0);
	old_uart = uart_base;
	if (verbose)
		printf("Old UART was 0x%x, new will be 0x%x\n", old_uart, new_uart);
	map_page(first_level_table, new_uart, uart_base, PRW_URW | EXECUTE_NEVER);
	uart_base = new_uart;

	if (verbose) {
		printf("We have made the swap\n");
		show_pages(kern_virt_allocator);
	}

	/*
	 * Here we unmap the old physical code locations, and the old UART
	 * location.
	 */
	unmap_pages(first_level_table, phys_code_start, phys_dynamic - phys_code_start);
	unmap_pages(first_level_table, old_uart, 0x1000);

	/*
	 * At this point, we can adjust the flags on the code to be read-only,
	 * and the flags on the data to be execute never. This is safer and
	 * generally a bit more secure.
	 */
	map_pages(first_level_table, (uint32_t) code_start, phys_code_start,
			(uint32_t)(code_end - code_start), PRO_URO);
	map_pages(first_level_table, (uint32_t) data_start, phys_data_start,
			((uint32_t)first_level_table + 0x00404000 - (uint32_t)data_start),
			PRW_URW | EXECUTE_NEVER);

	if (verbose)
		printf("We have adjusted memory permissions!\n");

	/*
	 * Setup stacks for other modes, and map the first code page at 0x00 so
	 * we can handle exceptions.
	 */
	stack = get_mapped_pages(4096, 0);
	setup_stacks(stack);

	/* This may be a no-op, but let's map the interrupt vector at 0x0 */
	map_page(first_level_table, 0x00, phys_code_start, PRO_URO);

	if (verbose) {
		printf("We have setup interrupt mode stacks!\n");
		/*print_first_level(0, 0xFFFFFFFF);*/
	}
}
