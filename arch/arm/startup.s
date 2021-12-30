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

/*
 * Hey look, it's an interrupt vector! Have some commentary about it:
 *
 * Code from startup.s resides at the beginning of the code section, on a page
 * boundary. We map this page to address 0x00 (in addition to the normal address
 * for this code) so that it can also serve as the interrupt vector.
 *
 * Code from entry.s resides directly after. It contains the _impls for these
 * branches, which are the Interrupt Service Routines. The linker will ensure
 * our ISRs reside completely within this first page.
 *
 * We should never really use the "reset" vector. I suspect this code might
 * break if it was used diretly.
 */
.globl _start
_start: b start_impl
_undefined: ldr pc, =undefined_impl
_swi: ldr pc, =swi_impl
_prefetch_abort: ldr pc, =prefetch_abort_impl
_data_abort: ldr pc, =data_abort_impl
_undefined_interrupt: ldr pc, =undefined_impl
_irq: ldr pc, =irq_impl
_fiq: ldr pc, =fiq_impl

start_impl:
	/* Are we in HYP mode? If so, get out of there */
	mrs r0, cpsr
	and r1, r0, #MODE_MASK
	cmp r1, #MODE_HYP
	bne out_of_hyp

	/* Set spsr to SVC mode */
	bic r0, #MODE_MASK
	orr r0, #MODE_SVC
	msr spsr_cxsf, r0
	/* Set HVBAR to our interrupt vector */
	mov r0, #0x8000
	mcr p15, 4, r0, c12, c0, 0
	/* Set LR to the next instructions */
	add r0, pc, #4
	msr ELR_hyp, r0
	eret

out_of_hyp:
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
