/*
 * arm - mmu.c
 *
 * Implementation of page table management routines for arm (32-bit)
 * architecture.
 */
#include <arch/cpu.h>
#include <arch/mmu.h>

#include "kernel.h"
#include "board.h"
#include "util.h"
#include "mm.h"
#include "arm-mmu.h"

#define KERN_START_MB (CONFIG_KERNEL_START >> 20)
#define VMLC_START_MB (VMALLOC_START >> 20)

#if CONFIG_KERNEL_START > 0x80000000
    #error "Kernel start > 0x80000000 is not supported by TTBR methods"
#endif

#define top_n_bits(n) (0xFFFFFFFF << (32 - n))
#define bot_n_bits(n) (0xFFFFFFFF >> (32 - n))

uintptr_t *first_level_table;

/*
 * Allocators.
 */
void *arch_phys_allocator;
uintptr_t arch_direct_map_offset;

/*
 * Physical addresses.
 */
uintptr_t phy_start;
uintptr_t phy_code_start;
uintptr_t phy_memsize;

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
static void init_second_level(uintptr_t *second)
{
	uintptr_t i;
	for (i = 0; i < 256; i++)
		second[i] = 0;
}

/**
 * Return the address of a second-level table from a first level descriptor
 */
static uintptr_t *get_second(uintptr_t fld)
{
	return (uintptr_t *)kptov(FLD_PAGE_TABLE(fld));
}

/**
 * Create a second-level table, given that it doesn't exist.
 */
static uintptr_t *create_second(uintptr_t *base, uintptr_t first_idx)
{
	uintptr_t *second, second_phys;
	/* for user memory, we allocate a page (4KB rather than 1KB, so
	* TODO we're wasting memory) of physical memory, map it into
	* kernel space, and we use that as a second-level table. */
	second = kmem_get_page();
	second_phys = kvtop(second);
	base[first_idx] = second_phys | FLD_COARSE;

	/*
	 * TODO: The mystery is weird here. From discussions about the memory
	 * model, it seems like I shouldn't need to clean the cache here,
	 * because the MMU should be cache coherent. But in practice, things
	 * crash if I don't clean the cache to PoC (PoU won't work even).
	 */
	DCCMVAC(&base[first_idx]);
	init_second_level(second);
	return second;
}

/**
 * Clean up all second-level tables in a user page table
 */
void arch_umem_free(void *as)
{
	uintptr_t *first = as;
	uintptr_t i, fld, *second;
	for (i = 0; i < CONFIG_KERNEL_START >> 20; i++) {
		fld = first[i];
		if ((fld & FLD_MASK) == FLD_COARSE) {
			second = get_second(fld);
			kmem_free_page(second);
		}
	}
}

/**
 * Insert a mapping from a virtual to a physical page. This is done via small
 * pages.
 *
 * @param base The virtual address of the first-level page table
 * @param virt virtual address to map (should be page aligned)
 * @param phys physical address (should be page aligned)
 * @param attrs second-level small page descriptor bits to include
 */
static void map_page(uintptr_t *base, uintptr_t virt, uintptr_t phys,
                     uintptr_t attrs)
{
	uintptr_t *second;
	uintptr_t fld = base[fld_idx(virt)];

	if ((fld & FLD_MASK) == FLD_COARSE) {
		second = get_second(fld);
	} else if ((fld & FLD_MASK) == 0) {
		second = create_second(base, virt >> 20);
	} else {
		puts("map_page: First level table entry doesn't point to table");
		return;
	}

	second[sld_idx(virt)] = (phys & 0xFFFFF000) | attrs | SLD_SMALL;
	/*
	 * TODO: The mystery is weird here. From discussions about the memory
	 * model, it seems like I shouldn't need to clean the cache here,
	 * because the MMU should be cache coherent. But in practice, things
	 * crash if I don't clean the cache to PoC (PoU won't work even).
	 */
	DCCMVAC(&second[sld_idx(virt)]);
}

/* Map multiple pages in a row. This is mainly to automate map_page() */
static void map_pages(uintptr_t *base, uintptr_t virt, uintptr_t phys,
                      uintptr_t len, uintptr_t attrs)
{
	uintptr_t i;
	for (i = 0; i < len; i += 4096)
		map_page(base, virt + i, phys + i, attrs);
}

void arch_umem_map(void *as, uintptr_t virt, uintptr_t phys,
                   uintptr_t len, enum umem_perm perm)
{
	uintptr_t *first = as;
	uintptr_t attrs;
	if (perm == UMEM_RO) {
		attrs = UMEM_FLAGS_RO;
	} else {
		attrs = UMEM_FLAGS_RW;
	}
	map_pages(first, virt, phys, len, attrs);
	mb();
	isb();
}

/**
 * Lookup virtual address of virt in page table.
 */
static uintptr_t lookup_phys(uintptr_t *base, void *virt_ptr)
{
	uintptr_t virt = (uintptr_t)virt_ptr;

	uintptr_t fld = base[fld_idx(virt)];
	if ((fld & FLD_MASK) == FLD_COARSE) {
		uintptr_t sld = get_second(fld)[sld_idx(virt)];
		if ((sld & SLD_MASK) == SLD_SMALL) {
			return SLD_ADDR(sld) | (0xFFF & virt);
		} else {
			/* TODO support other table types? */
			return 0;
		}
	} else if ((fld & FLD_MASK) == FLD_SECTION) {
		return FLD_SECTION_ADDR(fld) | (0xFFFFF & virt);
	} else {
		return 0;
	}
}

uintptr_t arch_umem_lookup(void *as, void *virt_ptr)
{
	return lookup_phys((uintptr_t *)as, virt_ptr);
}

/*
uintptr_t kmem_lookup_phys(void *virt_ptr)
{
	return lookup_phys(first_level_table, virt_ptr);
}
*/

void arch_devmap(uintptr_t virt, uintptr_t phys, uintptr_t len)
{
	map_pages(first_level_table, virt, phys, len, PERIPH_DEFAULT);
}

static inline uintptr_t get_kern_end_mb(uintptr_t memsize)
{
	uintptr_t mbs = memsize >> 20;
	uintptr_t end_mb = KERN_START_MB + mbs;
	if (end_mb > VMLC_START_MB)
		return VMLC_START_MB;
	return end_mb;
}

/**
 * Called before the MMU is enabled. Initializes the page tables to create the
 * following virtual memory map: (a 2/2 split)
 *
 * 0x0000 0000 - 0x0000 0FFF  Null page, used to catch bad memory accesses.
 * 0x0000 1000 - 0x7FFF FFFF  Userspace memory
 * 0x8000 0000 - 0xFF7F FFFF  Kernel direct-map memory
 * 0xFF80 0000 - 0xFFFF FFFF  Kernel "vmalloc" memory
 *
 * Kernel direct-map memory gets mapped to the start address and extends until
 * its full size, or if there is more than 2040 MiB of physical memory, then it
 * extends until 2040 MiB. The remaining 8 MiB are used for "vmalloc", which
 * means that it can be used to map virtual addresses to MMIO, or to temporarily
 * map physical pages which are beyond the capacity of the kernel address space.
 *
 * For its part, this function simply does the direct mapping and sets
 * everything else unmapped.
 */
int kmem_init_page_tables(uintptr_t start, uintptr_t size)
{
	uintptr_t i, end_mb;


	if (start & 0xFFFFF) {
		puts("Physical memory is not 1MB aligned");
		return -1;
	}
	if (size & 0xFFFFF) {
		puts("Memory size is not 1MB aligned");
		return -1;
	}

	end_mb = get_kern_end_mb(size);

	for (i = 0; i < KERN_START_MB; i++)
		first_level_table[i] = 0;
	for (i = KERN_START_MB; i < end_mb; i++) {
		first_level_table[i] = (start & 0xFFF00000) | KMEM_DEFAULT | FLD_SECTION;
		start += 1<<20;
	}
	for (; i < 4096; i++)
		first_level_table[i] = 0;
	return 0;
}

static void map_first_page_identity(void)
{
	uintptr_t idx = phy_start >> 20;
	first_level_table[idx] = (phy_start & 0xFFF00000) | KMEM_DEFAULT | FLD_SECTION;
}

static void unmap_first_page_identity(void)
{
	uintptr_t idx = phy_start >> 20;
	first_level_table[idx] = 0;
}

/**
 * Initialize page tables to the point where we can return to assembly and
 * enable the MMU. This means establishing the kernel direct mapping, and then
 * inserting a temporary identity map at the kernel load address.
 */
int arch_premmu(uintptr_t phys_start, uintptr_t memsize, uintptr_t code_load)
{
	int rv = 0;
	uintptr_t phy_dynamic_start;

	phy_start = phys_start;
	phy_code_start = code_load;
	phy_dynamic_start = code_load + ((void*)unused_start - (void*) code_start);
	phy_memsize = memsize;

	arch_phys_allocator = (void *)phy_dynamic_start;
	init_page_allocator(arch_phys_allocator, phys_start, phys_start + memsize);
	mark_alloc(arch_phys_allocator, phy_code_start, (uintptr_t)arch_phys_allocator + PAGE_SIZE);

	first_level_table = (uintptr_t *)alloc_pages(arch_phys_allocator, PAGE_SIZE, 12);

	/* Note any oddities with phy_start / phy_code_start */
	if (code_load > CONFIG_KERNEL_START) {
		puts("BUG: kernel loaded past CONFIG_KERNEL_START, we can't map vector pages");
		return -1;
	}

	arch_direct_map_offset = CONFIG_KERNEL_START - phys_start;
	rv = kmem_init_page_tables(phy_start, memsize);
	if (rv < 0)
		return rv;

	map_first_page_identity();

	/* load our first level into TTBR0 and 1 */
	set_ttbr0((uintptr_t)first_level_table);
	set_ttbr1((uintptr_t)first_level_table);
	set_ttbcr(1); /* 2/2 split */
	set_dacr(1);  /* client of domain 0 */

	/* symbol address == virtual address */
	set_vbar((uintptr_t)&code_start);

	return 0;
}

/**
 * Do memory initialization after MMU enabled.
 *
 * This is miscellaneous stuff - clean up the identity mappings, remove the page
 * allocators, and be sure the other processor modes have a stack pointer.
 */
int arch_postmmu(void)
{
	//void *stack;

	set_ttbr0(0); /* no need for this anymore */
	first_level_table = kptov((uintptr_t) first_level_table);
	arch_phys_allocator = kptov((uintptr_t) arch_phys_allocator);

	/*
	 * Remove the identity mapping which helped us swich from premmu to postmmu.
	 */
	unmap_first_page_identity();

	/*
	 * Setup stacks for other modes, and map the first code page at 0x00 so
	 * we can handle exceptions.
	 */
	//stack = kmem_get_pages(8192, 0);
	//setup_stacks(stack); /* asm; sets the stacks in each mode */
	//fiq_stack = stack + 1 * 1024;
	//abrt_stack = stack + 2 * 1024;
	//undf_stack = stack + 3 * 1024;
	//irq_stack = stack + 8 * 1024;
	//svc_stack = &stack_end;
	return 0;
}

static void print_second_level(uintptr_t *second, uintptr_t start, uintptr_t end)
{
	uintptr_t virt_base = start & top_n_bits(12);
	uintptr_t i = sld_idx(start);
	uintptr_t sld;

	while (start < end && i < 256) {
		sld = second[i];
		switch (sld & SLD_MASK) {
		case SLD_LARGE:
			printf("\t0x%x: 0x%x (large)\n", virt_base + (i << 12),
			       sld & top_n_bits(16));
			break;
		case 0x3:
		case SLD_SMALL:
			printf("\t0x%x: 0x%x (small), xn=%u, tex=%u, ap=%u, "
			       "apx=%u, ng=%u\n",
			       virt_base + (i << 12),
			       sld & top_n_bits(20), sld & 0x1,
			       (sld >> 6) & 0x7, (sld >> 4) & 0x3,
			       (sld & (1 << 9)) ? 1 : 0,
			       (sld & SLD_NG) ? 1 : 0);
			break;
		default:
			break;
		}
		i += 1;
		start = virt_base | (i << 12);
	}
}

void mem_print(uintptr_t *base, uintptr_t start, uintptr_t stop)
{
	uintptr_t i, fld, *second;
	for (i = start >> 20; i <= (stop >> 20); i++) {
		start = i << 20;
		fld = base[i];
		switch (fld & FLD_MASK) {
		case FLD_SECTION:
			printf("0x%x: SECTION 0x%x, domain=%u\n", i << 20,
			       FLD_SECTION_ADDR(fld),
			       (fld >> 5) & 0xF);
			break;
		case FLD_COARSE:
			second = get_second(fld);
			printf("0x%x: SECOND 0x%x phys / 0x%x virt, "
			       "domain=%u\n",
			       i << 20, FLD_PAGE_TABLE(fld), second,
			       (fld >> 5) & 0xF);
			print_second_level(get_second(fld), start, stop);
			break;
		case FLD_UNMAPPED:
		default:
			break;
		}
	}
}

void kmem_print(uintptr_t start, uintptr_t stop)
{
	mem_print(first_level_table, start, stop);
}

#if 0
static struct field sld_small_page_fields[] = {
	FIELD_BIT("B", 2),
	FIELD_BIT("C", 3),
	FIELD_2BIT("AP[1:0]", 4),
	FIELD_3BIT("TEX[2:0]", 6),
	FIELD_BIT("AP[2]", 9),
	FIELD_BIT("S", 10),
	FIELD_BIT("nG", 11),
	FIELD_MASK("S.P. Base", 0xFFFFF000),
};
static struct field fld_ttable_fields[] = {
	FIELD_BIT("PXN", 2),
	FIELD_BIT("NS", 3),
	FIELD_BIT("RS0", 4),
	FIELD_4BIT("Domain", 5),
	FIELD_BIT("impl. defined", 6),
	FIELD_MASK("Table Base", 0xFFFFFC00),
};

void vmem_diag(uintptr_t addr)
{
	uintptr_t ttbr, fld, sld, *first, *second;

	if (addr < CONFIG_KERNEL_START)
		ttbr = get_ttbr0();
	else
		ttbr = get_ttbr1();

	printf("  TTBR for this address: 0x%x\n", ttbr);
	if (ttbr & 0xFFFFFF00) {
		first = kptov(ttbr);
		printf("  vaddr of first level table: 0x%x\n", first);
	} else {
		return; /* nothing more to do */
	}

	fld = first[fld_idx(addr)];
	printf("  fld=0x%x\n", fld);
	if ((fld & FLD_MASK) == FLD_COARSE) {
		puts("  ... FLD_COARSE! Dissect:\n");
		dissect_fields(fld, fld_ttable_fields, nelem(fld_ttable_fields));
		second = get_second(fld);
		printf("  second vaddr=0x%x\n", second);
		sld = second[sld_idx(addr)];
		printf("  sld=0x%x\n", sld);
		if ((sld & SLD_MASK) == SLD_UNMAPPED) {
			puts("  ... unmapped SLD.\n");
		} else if ((sld & 2) == 2) {
			puts("  ... SLD_SMALL! Dissect:\n");
			dissect_fields(sld, sld_small_page_fields, nelem(sld_small_page_fields));
			printf("  TADA! phys=0x%x\n", SLD_ADDR(sld) | (0xFFF & addr));
		} else {
			printf("  ... unknown SLD type 0x%x\n", (sld & SLD_MASK));
		}
	} else if ((fld & FLD_MASK) == FLD_SECTION) {
		puts("  ... FLD_SECTION!\n");
		printf("  TADA! phys=0x%x\n", FLD_SECTION_ADDR(fld) | (0xFFFFF & addr));
	} else if ((fld & FLD_MASK) == 0) {
		puts("  ... unmapped FLD.\n");
	} else {
		puts("  ... unknown FLD.\n");
	}
}
#endif
