#ifndef AARCH64_MMU_H
#define AARCH64_MMU_H

#include <stdint.h>

#include "config.h"

extern uintptr_t arch_direct_map_offset;
extern void *arch_phys_allocator;

void *arch_devmap(uintptr_t virt_addr, uintptr_t phys_addr, uintptr_t size);
void *arch_devunmap(uintptr_t virt_addr, uintptr_t size);

#define VMALLOC_END   0xFFFFFFFFFFFFF000ULL
#define VMALLOC_START (VMALLOC_END - (((uintptr_t)CONFIG_VMALLOC_MBS) << 20))

#endif // AARCH64_MMU_H
