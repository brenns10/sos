#ifndef ARCH_H
#define ARCH_H

#include <stdint.h>

#include <arch/config.h>

void arch_premmu(uintptr_t phy_start, uintptr_t memsize, uintptr_t code_start);
void arch_postmmu(void);

#endif // ARCH_H
