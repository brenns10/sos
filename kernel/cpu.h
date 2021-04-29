#pragma once

#include <stdbool.h>
#include <stdint.h>

struct ctx {
	uint32_t sp;
	uint32_t lr;
	uint32_t a1;
	uint32_t a2;
	uint32_t a3;
	uint32_t a4;
	uint32_t r12;
	uint32_t v1;
	uint32_t v2;
	uint32_t v3;
	uint32_t v4;
	uint32_t v5;
	uint32_t v6;
	uint32_t v7;
	uint32_t v8;
	uint32_t ret;
	uint32_t spsr;
};

#define get_sp(dst) __asm__ __volatile__("mov %[rd], sp" : [rd] "=r"(dst) : :)
#define get_fp(dst) __asm__ __volatile__("mov %[rd], fp" : [rd] "=r"(dst) : :)

#define ARM_MODE_USER 0x10U
#define ARM_MODE_FIQ  0x11U
#define ARM_MODE_IRQ  0x12U
#define ARM_MODE_SVC  0x13U
#define ARM_MODE_ABRT 0x17U
#define ARM_MODE_HYP  0x1AU
#define ARM_MODE_UNDF 0x1BU
#define ARM_MODE_SYS  0x1FU
#define ARM_MODE_MASK 0x1FU

/**
 * Load coprocessor register into dst.
 */
#define get_cpreg(dst, CRn, op1, CRm, op2)                                     \
	__asm__ __volatile__("mrc p15, " #op1 ", %[rd], " #CRn ", " #CRm       \
	                     ", " #op2                                         \
	                     : [rd] "=r"(dst)                                  \
	                     :                                                 \
	                     :)

/**
 * Set coprocessor register from src.
 */
#define set_cpreg(src, CRn, op1, CRm, op2)                                     \
	__asm__ __volatile__("mcr p15, " #op1 ", %[rs], " #CRn ", " #CRm       \
	                     ", " #op2                                         \
	                     : [rs] "+r"(src)                                  \
	                     :                                                 \
	                     :)

#define set_cpreg2(src, opc1, CRn, CRm, opc2)                                  \
	set_cpreg(src, CRn, opc1, CRm, opc2)
#define get_cpreg2(src, opc1, CRn, CRm, opc2)                                  \
	get_cpreg(src, CRn, opc1, CRm, opc2)
/**
 * Get 64-bit cpreg
 */
#define get_cpreg64(dstlo, dsthi, CRm, op3)                                    \
	__asm__ __volatile__("mrrc p15, " #op3 ", %[Rt], %[Rt2], " #CRm        \
	                     : [Rt] "=r"(dstlo), [Rt2] "=r"(dsthi)             \
	                     :                                                 \
	                     :)

#define set_cpreg64(srclo, srchi, CRm, op3)                                    \
	__asm__ __volatile__("mcrr p15, " #op3 ", %[Rt], %[Rt2], " #CRm        \
	                     : [Rt] "+r"(srclo), [Rt2] "+r"(srchi)             \
	                     :                                                 \
	                     :)

/**
 * Load SPSR
 */
#define get_spsr(dst)                                                          \
	__asm__ __volatile__("mrs %[rd], spsr" : [rd] "=r"(dst) : :)

/**
 * Load CPSR
 */
#define get_cpsr(dst)                                                          \
	__asm__ __volatile__("mrs %[rd], cpsr" : [rd] "=r"(dst) : :)

#define mb()                __asm__ __volatile__("dsb")
#define interrupt_disable() __asm__ __volatile__("cpsid i")
#define interrupt_enable()  __asm__ __volatile__("cpsie i")

#define cpu_infinite_loop() __asm__ __volatile__("sub pc, pc, #8")

#define CPSR_I (1 << 7)
static inline bool interrupts_enabled(void)
{
	uint32_t cpsr;
	get_cpsr(cpsr);
	return !(cpsr & CPSR_I);
}

static inline void irqsave(int *flags)
{
	if (interrupts_enabled()) {
		*flags = 1;
		interrupt_disable();
	} else {
		*flags = 0;
	}
}

static inline void irqrestore(int *flags)
{
	if (*flags)
		interrupt_enable();
}

static inline void tlbiall(void)
{
	uint32_t reg = 0;
	set_cpreg(reg, c8, 0, c7, 0);
}

static inline uint32_t get_sctlr()
{
	uint32_t reg;
	get_cpreg2(reg, 0, c1, c0, 0);
	return reg;
}

/**
 * Data Cache Clean and Invalidate by Virtual Address
 * -> write back and invalidate cache for src
 */
static inline void DCCIMVAC(void *src)
{
	set_cpreg2(src, 0, c7, c14, 1);
}

/**
 * Instruction Cache Invalidate by Virtual Address
 * -> not sure why you'd want this to be honest
 */
static inline void ICIMVAU(void *src)
{
	set_cpreg2(src, 0, c7, c5, 1);
}

/**
 * Instruction Cache Invalidate All
 */
static inline void ICIALLU(void)
{
	uint32_t val = 1; /* don't actually care */
	set_cpreg2(val, 0, c7, c5, 0);
}

static inline void set_ttbr0(uint32_t val)
{
	set_cpreg2(val, 0, c2, c0, 0);
}
static inline void set_ttbr1(uint32_t val)
{
	set_cpreg2(val, 0, c2, c0, 1);
}
static inline void set_ttbcr(uint32_t val)
{
	set_cpreg2(val, 0, c2, c0, 2);
}
static inline void set_dacr(uint32_t val)
{
	set_cpreg2(val, 0, c3, c0, 0);
}
static inline void set_vbar(uint32_t val)
{
	set_cpreg2(val, 0, c12, c0, 0);
}
