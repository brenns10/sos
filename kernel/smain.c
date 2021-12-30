#include <arch/arch.h>

#include "kernel.h"

void pre_mmu(uintptr_t load_addr)
{
	puts("Hello world!\n");
	printf("Hello world! Loaded at 0x%x\n", load_addr);
	arch_premmu(0x40000000ULL, 0x40000000ULL, load_addr);
}

void main(uintptr_t addr)
{
        // pass
}
