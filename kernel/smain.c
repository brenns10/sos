#include "kernel.h"

void pre_mmu(uintptr_t load_addr)
{
	puts("Hello world!\n");
	printf("Hello world! Loaded at 0x%x\n", load_addr);
}

void main(uintptr_t addr)
{
        // pass
}
