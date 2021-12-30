#ifndef ARM_MMU_H
#define ARM_MMU_H

#include "config.h"

#define VMALLOC_END   0xFFFFF000
#define VMALLOC_START (VMALLOC_END - ((CONFIG_VMALLOC_MBS) << 20))

#endif // ARM_MMU_H
