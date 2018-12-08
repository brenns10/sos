/*
 * Startup stub
 */
.text

.globl _start
_start:
	/* Set stack pointer for C code */
	ldr sp, =stack_end

	/* Now, let's enable the MMU. This is implemented in C, yay! */
	bl enable_mmu

	/*
	 * OK, it gets a bit tricky here. We want to be running in "kernel
	 * memory space", above the user/kernel split at 0xC0000000. We know our
	 * code's destination address, and our current address, so we add that
	 * offset to the stack pointer, and pc.
	 */
	ldr r0, =#0xC0000000
	ldr r1, =_start
	sub r0, r0, r1
	add sp, sp, r0
	add pc, pc, r0

	/*
	 * Delay slot since pc's value is the next instruction. This also allows
	 * us to call main() with the address where physical code began, so that
	 * we know what physical memory is available.
	 */
	mov r0, r1

	/*
	 * And look, now we're running above 0xC0000000! How easy was that?
	 * We can clear out the old page descriptors now that we're totally in
	 * virtual memory :D
	 *
	 * C code down the line will never notice, because it's all compiled
	 * with -fPIC, so it's position independent.
	 */

	/* This address works because it's a local branch. */
	bl main

	/* Infinite loop at the end */
	sub pc, pc, #8
