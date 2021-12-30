#ifndef AARCH64_CPREG_H
#define AARCH64_CPREG_H

#include <stdint.h>

#define _msr(cpreg, val)			\
	__asm__ __volatile__("msr " #cpreg ", %[rs]" : [rs] "+r" (val) : :)

#endif // AARCH64_CPREG_H
