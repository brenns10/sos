.text

.global undefined_impl
undefined_impl: sub pc, pc, #8

.global swi_impl
swi_impl:
	/*
	 * Dump LR and SPSR to the kernel-mode stack.
	 * TODO: we could even dump this directly into current_process->context
	 */
	srsfd sp!, #0x13
	push {v1-v8}

	/* Save SP_usr and LR_usr */
	cps #0x1F  /* system */
	mov v1, sp
	mov v2, lr
	cps #0x13  /* svc */
	push {v1, v2}

	/* Look at the SWI instruction and get the interrupt number. */
	ldr v1, [lr, #-4]
	bic v1, v1, #0xFF000000

	adr lr, _swi_ret           /* set our return address */
	cmp v1, #2                 /* compare to max syscall number */
	movhi a1, v1               /* if higher, go to generic swi() with */
	bhi swi                    /* syscall number as arg */
	add pc, pc, v1, lsl #2     /* branch to pc + interrupt number * 4 */
	nop

	/* SYSTEM CALL TABLE */
	/* 0 */ b sys_relinquish
	/* 1 */ b sys_display
	/* 2 */ b sys_exit
	/* END. Please update max syscall number above. */

_swi_ret:
	/* Restore SP_usr and LR_usr */
	pop {v1, v2}
	cps #0x1F  /* system */
	mov sp, v1
	mov lr, v2
	cps #0x13  /* svc */

	pop {v1-v8}
	rfefd sp!

.global prefetch_abort_impl
prefetch_abort_impl:
	srsfd sp!, #0x17 /* abort */
	push {r0-r12}
	bl prefetch_abort
	nop  /* infinite loop since it broke */
	sub pc, pc, #8

.global irq_impl
irq_impl:
	srsfd sp!, #0x12 /* irq */
	push {r0-r12}
	bl irq
	nop  /* infinite loop since irq disabled */
	sub pc, pc, #8

.global fiq_impl
fiq_impl:
	srsfd sp!, #0x11
	push {r0-r12} /* lol these are probably not the right registers */
	bl fiq
	nop  /* infinite loop since it what even is fiq */
	sub pc, pc, #8

.global data_abort_impl
data_abort_impl:
	srsfd sp!, #0x17 /* abort */
	push {r0-r12}
	bl data_abort
	nop  /* infinite loop since it broke */
	sub pc, pc, #8

/*
 * Change into each mode, and set stack to be 1024 bytes of the page pointed by
 * a1.
 */
.global setup_stacks
setup_stacks:
	cps #0x11
	add a1, a1, #1024
	mov sp, a1
	cps #0x12
	add a1, a1, #1024
	mov sp, a1
	cps #0x17
	add a1, a1, #1024
	mov sp, a1
	cps #0x1B
	add a1, a1, #1024
	mov sp, a1
	cps #0x13
	mov pc, lr

/*
 * A "system call" whose sole purpose is to return control to the kernel, and
 * then resume once we're resumed again.
 */
.global relinquish
relinquish:
	svc #0
	mov pc, lr

/*
 * Start a process running, assembly helper.
 * a1: place to branch
 * a2: stack pointer
 */
.global start_process_asm
start_process_asm:
	/* reset the stack pointer cause we don't need it any more */
	ldr sp, =stack_end
	cps #0x10
	mov sp, a2
	mov pc, a1
