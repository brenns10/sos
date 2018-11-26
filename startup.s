/*
 * Startup stub
 */
.text

.globl _start
_start:
	/* Set stack pointer to top of stack, grows downward.
	 * Top of stack is defined in the link script.
	 */
	ldr sp, =stack_start

	bl main

	/* Infinite loop at the end */
loop:
	b loop
