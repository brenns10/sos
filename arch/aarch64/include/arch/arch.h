#ifndef ARCH_H
#define ARCH_H

#include <stdint.h>

#define ARCH aarch64
#define ARCH_64BIT
#undef  ARCH_32BIT
#define ARCH_BITS 64

void arch_premmu(uintptr_t phy_start, uintptr_t memsize, uintptr_t code_start);
void arch_postmmu(void);

#endif // ARCH_H
