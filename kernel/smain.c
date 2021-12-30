#include <arch/arch.h>

#include "kernel.h"
#include "mm.h"

void pre_mmu(uintptr_t load_addr)
{
	puts("Hello world!\n");
	printf("Hello world! Loaded at 0x%x\n", load_addr);
	arch_premmu(0x40000000ULL, 0x40000000ULL, load_addr);
	puts("Finished arch_premmu()\n");
}

void main(uintptr_t addr)
{
	arch_postmmu();
	kmem_init();
	uart_remap();
        puts("Hello world, postmmu!\n");
}
