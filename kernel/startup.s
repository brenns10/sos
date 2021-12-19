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

.globl _start
_start:
        nop // 0x000
        nop // 0x004
        nop // 0x008
        nop // 0x00c
        nop // 0x010
        nop // 0x014
        nop // 0x018
        nop // 0x01c
        nop // 0x020
        nop // 0x024
        nop // 0x028
        nop // 0x02c
        nop // 0x030
        nop // 0x034
        nop // 0x038
        nop // 0x03c
        nop // 0x040
        nop // 0x044
        nop // 0x048
        nop // 0x04c
        nop // 0x050
        nop // 0x054
        nop // 0x058
        nop // 0x05c
        nop // 0x060
        nop // 0x064
        nop // 0x068
        nop // 0x06c
        nop // 0x070
        nop // 0x074
        nop // 0x078
        nop // 0x07c
_irq:
        nop // 0x080
        nop // 0x084
        nop // 0x088
        nop // 0x08c
        nop // 0x090
        nop // 0x094
        nop // 0x098
        nop // 0x09c
        nop // 0x0a0
        nop // 0x0a4
        nop // 0x0a8
        nop // 0x0ac
        nop // 0x0b0
        nop // 0x0b4
        nop // 0x0b8
        nop // 0x0bc
        nop // 0x0c0
        nop // 0x0c4
        nop // 0x0c8
        nop // 0x0cc
        nop // 0x0d0
        nop // 0x0d4
        nop // 0x0d8
        nop // 0x0dc
        nop // 0x0e0
        nop // 0x0e4
        nop // 0x0e8
        nop // 0x0ec
        nop // 0x0f0
        nop // 0x0f4
        nop // 0x0f8
        nop // 0x0fc
_fiq:
        nop // 0x100
        nop // 0x104
        nop // 0x108
        nop // 0x10c
        nop // 0x110
        nop // 0x114
        nop // 0x118
        nop // 0x11c
        nop // 0x120
        nop // 0x124
        nop // 0x128
        nop // 0x12c
        nop // 0x130
        nop // 0x134
        nop // 0x138
        nop // 0x13c
        nop // 0x140
        nop // 0x144
        nop // 0x148
        nop // 0x14c
        nop // 0x150
        nop // 0x154
        nop // 0x158
        nop // 0x15c
        nop // 0x160
        nop // 0x164
        nop // 0x168
        nop // 0x16c
        nop // 0x170
        nop // 0x174
        nop // 0x178
        nop // 0x17c
_serror:
        nop // 0x180
        nop // 0x184
        nop // 0x188
        nop // 0x18c
        nop // 0x190
        nop // 0x194
        nop // 0x198
        nop // 0x19c
        nop // 0x1a0
        nop // 0x1a4
        nop // 0x1a8
        nop // 0x1ac
        nop // 0x1b0
        nop // 0x1b4
        nop // 0x1b8
        nop // 0x1bc
        nop // 0x1c0
        nop // 0x1c4
        nop // 0x1c8
        nop // 0x1cc
        nop // 0x1d0
        nop // 0x1d4
        nop // 0x1d8
        nop // 0x1dc
        nop // 0x1e0
        nop // 0x1e4
        nop // 0x1e8
        nop // 0x1ec
        nop // 0x1f0
        nop // 0x1f4
        nop // 0x1f8
        nop // 0x1fc

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
	/* Trap cores which arent core 0 */
	mrc p15, 0, r0, c0, c0, 5
	and r0, #0xFF
	cmp r0, #0
	subne pc, pc, #8

	/*
	 * Step 0: Setup the stack pointer and branch into C code for pre_mmu
	 * initialization.
	 */
	adr a1, _start
	ldr a2, =code_start
	ldr a3, =stack_end
	sub a3, a3, a2
	add sp, a1, a3
	bl pre_mmu

	/*
	 * At this point, the page tables are initilaized by the pre_mmu code.
	 * Further, TTBCR and TTBR are set. All we need to do is enable the MMU
	 * and then reset the stack pointer and head into C land again.
	 */
	/*
	 * Step 4: Enable the MMU!
	 */
	mrc p15, 0, a1, c1, c0, 0
        mov a2, #1
	orr a1, a1, a2
	mcr p15, 0, a1, c1, c0, 0 /* set vmsa6 bit, set mmu enable */

	adr a1, _start /* put physical address of start as first arg to main */
	/*
	 * Step 4.5: Branch above the kernel/user space split. We use ldr to
	 * ensure that we are actually loading the linker-provided address for
	 * trampoline, which should be the virtual address.
	 *
	 * This ensures that when we branch and link, the return address on the
	 * stack will be in kernel-space and not user space.
	 */
	ldr pc, =_trampoline
.globl _trampoline
_trampoline:

	/*
	 * Step 5: Branch into C code! (after the stack pointer is set)
	 */
	ldr sp, =stack_end
	bl main

	/*
	 * Step 6: Infinite loop at end.
	 */
	sub pc, pc, #8
