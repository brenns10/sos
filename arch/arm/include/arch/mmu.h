#ifndef ARM_MMU_H
#define ARM_MMU_H

#include <stdint.h>

#include "config.h"

extern uintptr_t arch_direct_map_offset;
extern void *arch_phys_allocator;
enum umem_perm;

void arch_umem_free(void *as);
void arch_umem_map(void *as, uintptr_t virt, uintptr_t phys,
                   uintptr_t len, enum umem_perm perm);
uintptr_t arch_umem_lookup(void *as, void *virt_ptr);

void arch_devmap(uintptr_t virt_addr, uintptr_t phys_addr, uintptr_t size);
void arch_devunmap(uintptr_t virt_addr, uintptr_t size);

#define VMALLOC_END   0xFFFFF000
#define VMALLOC_START (VMALLOC_END - ((CONFIG_VMALLOC_MBS) << 20))

#endif // ARM_MMU_H
