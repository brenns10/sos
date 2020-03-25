/*
 * Memory routines and initialization
 */
#include "kernel.h"

#define top_n_bits(n) (0xFFFFFFFF << (32 - n))
#define bot_n_bits(n) (0xFFFFFFFF >> (32 - n))

/**
 * mem: unifies the way we treat user and kernel memory.
 *
 * Kernel memory maps everythng using coarse FLD's, and small pages in the SLD.
 * All second-level tables are pre-allocated ahead of time directly after the
 * end of the first-level table.  Thus, table mappings in the first-level table
 * need not be used (they have been pre-populated by the startup.s logic to
 * point to the correct physical addresses).
 *
 * User memory also maps everything using coarse FLD's, and small pages in the
 * SLD. However, we don't pre-allocate all second-level tables (this is much
 * memory). Since the first-level table contains physical addresses, we also
 * need a "shadow page table" to record virtual addresses of each second-level
 * table.
 *
 * With this struct, we can have all the info necessary, and delegate to proper
 * functions for doing tasks.
 */
struct mem {
	uint32_t *base;
	uint32_t **shadow;

	#define STRAT_KERNEL 1
	#define STRAT_SHADOW 2
	uint32_t strategy;
};

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

/*
 * Top-of-stack pointers, initialized by kmem_init()
 */
void *fiq_stack;
void *irq_stack;
void *abrt_stack;
void *undf_stack;
void *svc_stack;

/**
 * Initialize second level page descriptor table to an entirely empty mapping.
 */
static void init_second_level(uint32_t *second)
{
	uint32_t i;
	for (i = 0; i < 256; i++)
		second[i] = 0;
}

/**
 * Return the address of a second-level table, assuming it already exists.
 */
static uint32_t *get_second(struct mem *mem, uint32_t first_idx)
{
	if (mem->strategy == STRAT_KERNEL) {
		return second_level_table + (first_idx * 1024);
	} else {
		return (uint32_t*)mem->shadow[first_idx];
	}
}

/**
 * Create a second-level table, given that it doesn't exist.
 */
static uint32_t *create_second(struct mem *mem, uint32_t first_idx)
{
	uint32_t *second, second_phys;
	if (mem->strategy == STRAT_KERNEL) {
		/* for kernel memory, we simply reset it to "mapped" and return
		 * the proper second level location */
		mem->base[first_idx] &= ~FLD_MASK;
		mem->base[first_idx] |= FLD_COARSE;
		second = second_level_table + (first_idx * 1024);
		init_second_level(second);
	} else {
		/* for user memory, we allocate a page (4KB rather than 1KB, so
		 * TODO we're wasting memory) of physical memory, map it into
		 * kernel space, and we use that as a second-level table. */
		second = (uint32_t *)kmem_get_pages(0x1000, 0);
		second_phys = kmem_lookup_phys(second);
		mem->shadow[first_idx] = second;
		mem->base[first_idx] = second_phys | FLD_COARSE;
		init_second_level(second);
	}
	return second;
}

/**
 * Release a second-level table, assuming it has been emptied of entries.
 */
static void destroy_second(struct mem *mem, uint32_t first_idx)
{
	if (mem->strategy == STRAT_KERNEL) {
		mem->base[first_idx] &= ~FLD_MASK;
		mem->base[first_idx] |= FLD_UNMAPPED;
	} else {
		mem->base[first_idx] = 0;
		kmem_free_pages(mem->shadow[first_idx], 0x1000);
		mem->shadow[first_idx] = NULL;
	}
}

/**
 * Insert a mapping from a virtual to a physical page.
 *
 * virt: physical virtual address (should be page aligned)
 * phys: physical virtual address (should be page aligned)
 * attrs: access control attributes
 */
static void map_page(struct mem *mem, uint32_t virt, uint32_t phys, uint32_t attrs)
{
	uint32_t *second;
	uint32_t first_idx = virt >> 20;
	uint32_t second_idx = (virt >> 12) & 0xFF;

	if ((mem->base[first_idx] & FLD_MASK) != FLD_COARSE) {
		second = create_second(mem, first_idx);
	} else {
		second = get_second(mem, first_idx);
	}

	if (mem->strategy == STRAT_SHADOW)
		attrs |= SLD_NG;

	second[second_idx] = (phys & 0xFFFFF000) | attrs | SLD_SMALL;
}

void umem_map_page(struct process *p, uint32_t virt, uint32_t phys, uint32_t attrs)
{
	struct mem mem;
	mem.base = p->first;
	mem.shadow = p->shadow;
	mem.strategy = STRAT_SHADOW;
	map_page(&mem, virt, phys, attrs);
}

void kmem_map_page(uint32_t virt, uint32_t phys, uint32_t attrs)
{
	struct mem mem;
	mem.base = first_level_table;
	mem.strategy = STRAT_KERNEL;
	map_page(&mem, virt, phys, attrs);
}

/**
 * Lookup virtual address of virt in page table.
 */
static uint32_t lookup_phys(struct mem *mem, void *virt_ptr)
{
	uint32_t virt = (uint32_t) virt_ptr;
	uint32_t first_idx = virt >> 20;
	uint32_t second_idx = (virt >> 12) & 0xFF;
	uint32_t *second = get_second(mem, first_idx);
	return (
		(second[second_idx] & top_n_bits(20)) |
		(virt & bot_n_bits(12))
	);
}

uint32_t umem_lookup_phys(struct process *p, void *virt_ptr)
{
	struct mem mem;
	mem.base = p->first;
	mem.shadow = p->shadow;
	mem.strategy = STRAT_SHADOW;
	return lookup_phys(&mem, virt_ptr);
}

uint32_t kmem_lookup_phys(void *virt_ptr)
{
	struct mem mem;
	mem.base = first_level_table;
	mem.strategy = STRAT_KERNEL;
	return lookup_phys(&mem, virt_ptr);
}

void kmem_map_pages(uint32_t virt, uint32_t phys, uint32_t len, uint32_t attrs)
{
	uint32_t *base = first_level_table;
	uint32_t i;
	for (i = 0; i < len; i += 4096)
		kmem_map_page(virt + i, phys + i, attrs);
}

void umem_map_pages(struct process *p, uint32_t virt, uint32_t phys, uint32_t len, uint32_t attrs)
{
	uint32_t *base = first_level_table;
	uint32_t i;
	for (i = 0; i < len; i += 4096)
		umem_map_page(p, virt + i, phys + i, attrs);
}

static bool unmap_second(uint32_t *second, uint32_t start, uint32_t len)
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
		if ((second[i] & SLD_MASK) != SLD_UNMAPPED)
			return false;
	return true;
}

/**
 * Unmap pages beginning at start, for a total length of len.
 */
static void unmap_pages(struct mem *mem, uint32_t start, uint32_t len)
{
	/*
	 * First, unmap any second level descriptors before the first 1MB
	 * boundary, if there are any.
	 */
	if (start & bot_n_bits(20)) {
		uint32_t first_idx = start >> 20;
		uint32_t to_unmap;
		uint32_t next_mb = (start & top_n_bits(12)) + (1 << 20);
		uint32_t *second = get_second(mem, first_idx);;
		if (next_mb - start < len)
			to_unmap = next_mb - start;
		else
			to_unmap = len;
		if (unmap_second(second, start, to_unmap)) {
			destroy_second(mem, first_idx);
		}
		len -= to_unmap;
		start = next_mb;
	}

	/*
	 * Next, unmap all 1MB regions, if there are any. We erase their
	 * second-level tables too, for safety, but it's not really necessary,
	 * because kmem_map_pages() will erase them when in re-maps a 1MB block.
	 */
	while (len > 0x00100000) {
		uint32_t idx = start >> 20;
		if ((mem->base[idx] & FLD_MASK) != FLD_UNMAPPED) {
			destroy_second(mem, idx);
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
		uint32_t *second = get_second(mem, first_idx);
		if (unmap_second(second, start, len)) {
			destroy_second(mem, first_idx);
		}
	}
}

void umem_unmap_pages(struct process *p, uint32_t virt, uint32_t len)
{
	struct mem mem;
	mem.base = p->first;
	mem.shadow = p->shadow;
	mem.strategy = STRAT_SHADOW;
	unmap_pages(&mem, virt, len);
}

void kmem_unmap_pages(uint32_t virt, uint32_t len)
{
	struct mem mem;
	mem.base = first_level_table;
	mem.strategy = STRAT_KERNEL;
	unmap_pages(&mem, virt, len);
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
static void print_second_level(uint32_t *second, uint32_t start, uint32_t end)
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
				printf("\t0x%x: 0x%x (small), xn=%u, tex=%u, ap=%u, apx=%u, ng=%u\n",
					virt_base + (i << 12),
					second[i] & top_n_bits(20),
					second[i] & 0x1,
					(second[i] >> 6) & 0x7,
					(second[i] >> 4) & 0x3,
					(second[i] & (1 << 9)) ? 1 : 0,
					(second[i] & SLD_NG) ? 1 : 0
				);
				break;
			default:
				break;
		}
		i += 1;
		start = virt_base | (i << 12);
	}
}

static void print_first_level(struct mem *mem, uint32_t start, uint32_t stop)
{
	uint32_t i = start >> 20;
	uint32_t stop_idx = stop >> 20;
	uint32_t *second;
	while (i <= stop_idx) {
		switch (mem->base[i] & FLD_MASK) {
			case FLD_SECTION:
				printf("0x%x: SECTION 0x%x, domain=%u\n",
					i << 20,
					mem->base[i] & top_n_bits(10),
					(mem->base[i] >> 5) & 0xF
				);
				break;
			case FLD_COARSE:
				second = get_second(mem, i);
				printf("0x%x: SECOND 0x%x phys / 0x%x virt, domain=%u\n",
					i << 20,
					mem->base[i] & top_n_bits(22),
					second,
					(mem->base[i] >> 5) & 0xF
				);
				print_second_level(
					get_second(mem, i),
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

void kmem_print(uint32_t start, uint32_t stop)
{
	struct mem mem;
	mem.base = first_level_table;
	mem.strategy = STRAT_KERNEL;
	print_first_level(&mem, start, stop);
}

void umem_print(struct process *p, uint32_t start, uint32_t stop)
{
	struct mem mem;
	mem.base = p->first;
	mem.shadow = p->shadow;
	mem.strategy = STRAT_SHADOW;
	print_first_level(&mem, start, stop);
}

/**
 * Get mapped pages. That is, allocate some physical memory, allocate some
 * virtual memory, and map the virtual to the physical and return it.
 * bytes: count of bytes to allocate (increments of 4096 bytes)
 * align: alignment (only applied to the virtual address allocation)
 */
void *kmem_get_pages(uint32_t bytes, uint32_t align)
{
	void *virt = (void*)alloc_pages(kern_virt_allocator, bytes, align);
	uint32_t phys = alloc_pages(phys_allocator, bytes, 0);
	kmem_map_pages((uint32_t) virt, phys, bytes, PRW_UNA | EXECUTE_NEVER);
	return virt;
}

/**
 * Simpler method for use with slab
 */
void *kmem_get_page(void)
{
	return kmem_get_pages(4096, 0);
}

/**
 * Free memory which was allocated via kmem_get_pages(). This involves:
 * 1. Determine the physical address, we can do this via a software page table
 *    walk.
 * 2. Free the memory segments from the physical and virtual allocators.
 * 3. Unmap the virtual memory range.
 *
 * virt_ptr: virtual address pointer (must be page aligned)
 * len: length (must be page aligned)
 */
void kmem_free_pages(void *virt_ptr, uint32_t len)
{
	uint32_t phys = kmem_lookup_phys(virt_ptr);
	uint32_t virt = (uint32_t) virt_ptr;

	free_pages(phys_allocator, phys, len);
	free_pages(kern_virt_allocator, virt, len);

	kmem_unmap_pages(virt, len);
}

void kmem_free_page(void *ptr)
{
	kmem_free_pages(ptr, 4096);
}

void kmem_init(uint32_t phys, bool verbose)
{
	uint32_t new_uart, old_uart, alloc_so_far, cpreg;
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
	kmem_map_pages((uint32_t) dynamic, phys_dynamic, 0x2000, PRW_UNA | EXECUTE_NEVER);

	alloc_so_far = phys_dynamic - phys_code_start + 0x2000;
	init_page_allocator(phys_allocator, phys_code_start, 0xFFFFFFFF);
	mark_alloc(phys_allocator, phys_code_start, alloc_so_far);
	init_page_allocator(kern_virt_allocator, (uint32_t) code_start, 0x3FFFFFFF);
	mark_alloc(kern_virt_allocator, (uint32_t) code_start, alloc_so_far);

	if (verbose) {
		printf("We have allocators now!\n");
		/*show_pages(phys_allocator);
		show_pages(kern_virt_allocator);*/
	}

	/*
	 * Now that we have our allocators, let's allocate some virtual memory
	 * to map the UART at.
	 */
	new_uart = alloc_pages(kern_virt_allocator, 0x1000, 0);
	old_uart = uart_base;
	if (verbose)
		printf("Old UART was 0x%x, new will be 0x%x\n", old_uart, new_uart);
	kmem_map_page(new_uart, uart_base, PRW_UNA | EXECUTE_NEVER);
	uart_base = new_uart;

	if (verbose) {
		printf("We have made the swap\n");
		/*show_pages(kern_virt_allocator);*/
	}

	/*
	 * Here we unmap the old physical code locations, and the old UART
	 * location.
	 */
	kmem_unmap_pages(phys_code_start, phys_dynamic - phys_code_start);
	kmem_unmap_pages(old_uart, 0x1000);

	/*
	 * At this point, we can adjust the flags on the code to be read-only,
	 * and the flags on the data to be execute never. This is safer and
	 * generally a bit more secure.
	 */
	kmem_map_pages((uint32_t) code_start, phys_code_start,
			(uint32_t)(code_end - code_start), PRO_URO);
	kmem_map_pages((uint32_t) data_start, phys_data_start,
			((uint32_t)first_level_table + 0x00404000 - (uint32_t)data_start),
			PRW_UNA | EXECUTE_NEVER);

	if (verbose)
		printf("We have adjusted memory permissions!\n");

	/*
	 * Setup stacks for other modes, and map the first code page at 0x00 so
	 * we can handle exceptions.
	 */
	stack = kmem_get_pages(8192, 0);
	setup_stacks(stack); /* asm; sets the stacks in each mode */
	fiq_stack = stack + 1 * 1024;
	abrt_stack = stack + 2 * 1024;
	undf_stack = stack + 3 * 1024;
	irq_stack = stack + 8 * 1024;
	svc_stack = &stack_end;

	/* This may be a no-op, but let's map the interrupt vector at 0x0 */
	kmem_map_page(0x00, phys_code_start, PRO_URO);

	if (verbose) {
		printf("We have setup interrupt mode stacks!\n");
		/*print_first_level(0, 0xFFFFFFFF);*/
	}

	/* At this point, we have configured the first and second level tables
	 * as if they were managing the whole memory space. However, we have now
	 * successfully moved all virtual memory mappings to the beginning of
	 * the address space, and so we can use TTBR0 for kernel mappings up to
	 * 0x3FFFFFFF, and let 0x40000000+ be managed by TTBR1 for process
	 * mappings.
	 *
	 * This means several things:
	 * (a) the top 3/4 of the second level tables are wasted
	 * (b) the top 3/4 of the first level table is wasted
	 * (c) we need to set TTBR0 and TTBCR accordingly
	 *
	 * (a) and (b) are addressed by marking the respective regions now as
	 * free on our allocators, and unmapping them. We do (c) after that.
	 */
	kmem_unmap_pages((uint32_t)first_level_table + 0x1000, 0x3000);
	kmem_unmap_pages((uint32_t)second_level_table + 0x00100000, 0x00300000);
	free_pages(phys_allocator, (uint32_t)phys_first_level_table + 0x1000, 0x3000);
	free_pages(phys_allocator, (uint32_t)phys_second_level_table + 0x00100000, 0x00300000);
	free_pages(kern_virt_allocator, (uint32_t)first_level_table + 0x1000, 0x3000);
	free_pages(kern_virt_allocator, (uint32_t)second_level_table + 0x00100000, 0x00300000);

	cpreg = 2;
	set_cpreg(cpreg, c2, 0, c0, 2);

	if (verbose) {
		printf("We set TTBCR and now we're fully in kernel space!\n");
		printf("Here's the physical memory:\n");
		show_pages(phys_allocator);
		printf("Here's the kernel virtual memory:\n");
		show_pages(kern_virt_allocator);
	}
}
