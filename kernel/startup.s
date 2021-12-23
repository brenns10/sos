/*
 * startup.s: Kernel entry point. Does low-level MMU initialization and
 * branching into the correct memory location. Sets up stackc. Calls C main().
 *
 * This stub requires linker symbols:
 *
 *   - code_start: the VIRTUAL address where _start should end up mapped to
 *   - first_level_table: The VIRTUAL address where the first level page table
 *     should end up. This is directly after code, data, and stack.
 *
 * The physical address at which this code is loaded should not matter, provided
 * it is aligned to a 16KB (0x4000) boundary.
 *
 * No routine in this file should make use of the stack, all arguments should be
 * passed in registers. This speeds things up and prevents us from having to
 * setup the stack until after MMU is enabled.
 */
.equ MODE_USER, 0x10
.equ MODE_FIQ,  0x11
.equ MODE_IRQ,  0x12
.equ MODE_SVC,  0x13
.equ MODE_ABRT, 0x17
.equ MODE_HYP,  0x1A
.equ MODE_UNDF, 0x1B
.equ MODE_SYS,  0x1F
.equ MODE_MASK, 0x1F
.text

.align 7 // 0x80
.globl _start
_start: // this is not true: this is synchronous abort vector ...
        // D1.9.1: PE State on reset to AArch64 state:
        //  * PSTATE.{D,A,I,F} are 1 (all interrupts are masked)
        //  * All general purpose, SIMD, FP registers are unknown
        //  * All stack pointers, ELR, SPSR, for each EL are unknown
        //  * TLB & Caches are in implementation defined state
        //  * Timers are disabled
        //  * CurrentEL is the highest implemented EL
        b start_impl

.align 7 // 0x80
_irq:
        nop

.align 7 // 0x80
_fiq:
        nop

.align 7 // 0x80
_serror:
        nop

start_impl:
        /* Are we in HYP mode? If so, get out of there */
        mrs x0, CurrentEL
        cmp x0, #0b1100
        b.ne not_el3

        ldr x0, =continue
        msr ELR_EL3, x0

not_el3:
        cmp x0, #0b1000
        b.ne not_el2

        ldr x0, =continue
        msr ELR_EL2, x0
        // Mask FIQ, IRQ, Async Abort. Go to EL1h mode.
        mov x0, #0b111000101

not_el2:
        cmp x0, #0b0100
        b.ne el0

        // We're in EL1. This is the mode we want to be in!
        b continue

el0:
        // THIS IS AN ERROR, SHOULD NEVER HAPPEN


continue:
	/*
	 * Step 0: Setup the stack pointer and branch into C code for pre_mmu
	 * initialization.
	 */
	adr x1, _start
	ldr x2, =code_start
	ldr x3, =stack_end
	sub x3, x3, x2
	add sp, x1, x3
	bl pre_mmu
1:
        b 1b
