.text

.global undefined_impl
undefined_impl: sub pc, pc, #8

/*
 * Handle SWI. To do this we must store the process context and then dispatch
 * the correct function based on the system call number.
 *
 * We dump the process context onto the kernel-mode stack dedicated for that
 * process.
 */
.global swi_impl
swi_impl:
	/* Load up the kernel mode stack for this process */
	ldr sp, =current
	ldr sp, [sp]
	ldr sp, [sp]
	/*
	 * Dump LR and SPSR to the kernel-mode stack.
	 */
	srsfd sp!, #0x13
	push {v1-v8}
	push {a2-a4,r12} /* not guaranteed to be saved but we do */
	push {a1} /* will be clobbered by retval but stored anyway */

	/* Save SP_usr and LR_usr */
	cps #0x1F  /* system */
	mov v1, sp
	mov v2, lr
	cps #0x13  /* svc */
	push {v1, v2}

	/*
	 * Re-enable interrupts. When a system call is triggered, interrupts
	 * are disabled. However, our interrupt handler can safely interrupt
	 * the kernel and return, so we have no reason to leave them disabled.
	 *
	 * However, when we interrupt the kernel, under no circumstances will we
	 * reschedule. This means that a process spending a lot of time asking
	 * the kernel to run code on its behalf, could eat up more than its fair
	 * share of CPU time.
	 *
	 * To ensure that we don't reschedule, we must ensure interrupts are
	 * re-enabled AFTER we pop into SYS mode to grab the SP and LR -- this
	 * mode is reserved for kthreads, and if the IRQ handler sees this, it
	 * could try to reschedule us.
	 */
	cpsie i

	/* Look at the SWI instruction and get the interrupt number. */
	ldr v1, [lr, #-4]
	bic v1, v1, #0xFF000000

	adr lr, _swi_ret           /* set our return address */
	cmp v1, #5                 /* compare to max syscall number */
	movhi a1, v1               /* if higher, go to generic swi() with */
	bhi sys_unknown            /* syscall number as arg */
	add pc, pc, v1, lsl #2     /* branch to pc + interrupt number * 4 */
	nop

	/* SYSTEM CALL TABLE */
	/* 0 */ b sys_relinquish
	/* 1 */ b sys_display
	/* 2 */ b sys_exit
	/* 3 */ b sys_getchar
	/* 4 */ b sys_runproc
	/* 5 */ b sys_getpid
	/* END. Please update max syscall number above. */

_swi_ret:
	mov a2, #1 /* please use return value */
	mov a3, sp
	b return_from_exception

.global prefetch_abort_impl
prefetch_abort_impl:
	srsfd sp!, #0x17 /* abort */
	push {r0-r12}
	mov a1, lr
	bl prefetch_abort
	nop  /* infinite loop since it broke */
	sub pc, pc, #8

/**
 * Handle IRQ.
 *
 * An IRQ may occur while in any processor mode -- user or kernel. We need to
 * handle it quickly and return to whatever we were doing before. However, IRQ
 * could be used as an opportunity to schedule, so we need to be able to change
 * the currently running process as well.
 */
.global irq_impl
irq_impl:
	/* Dump context directly into current->context */
	ldr sp, =current
	ldr sp, [sp]
	add sp, sp, #72

	/*
	 * The lr points at the interrupted instruction. To resume, reset it
	 * back by one instruction.
	 * NOTE: assumes that we don't have Thumb instructions
	 */
	add lr, lr, #-4

        /* Dump LR and SPSR to current->context. */
	srsfd sp!, #0x12 /* irq */

	/* Save registers in standard order */
	push {v1-v8}
	push {a2-a4,r12}
	push {a1}

	/*
	 * Save SP and LR of interrupted mode. To do this most correctly, we
	 * need to conditionally switch into a mode where we can grab those
	 * registers, copy them, and then pop them. So we load spsr and check
	 * the mode we interrupted, and do that.
	 */
	mrs v1, spsr
	and v1, v1, #0x1F
	cmp v1, #0x13 /* SVC */
	bne 1f
	cps #0x13 /* SVC */
	b 2f
	1:
	cps #0x1F  /* system */
	2:
	mov v1, sp
	mov v2, lr

	/* Now return to IRQ mode and push those registers to the stack */
	cps #0x12  /* irq */
	push {v1, v2}

	/* Our context is fully saved, now let us load the real IRQ stack
	 * pointer. */
	ldr sp, =irq_stack
	ldr sp, [sp]

	/* Branch into the C IRQ handler. */
	bl irq
	mov a2, #0  /* please don't use return value */
	/* On return, we again restore current->context */
	ldr a3, =current
	ldr a3, [a3]
	add a3, a3, #4
	b return_from_exception

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
	mov a1, lr
	bl data_abort
	nop  /* infinite loop since it broke */
	sub pc, pc, #8

/*
 * Change into each mode, and set stack to be 1024 bytes of the page pointed by
 * a1.
 */
.global setup_stacks
setup_stacks:
	cps #0x11  /* MODE_FIQ */
	add a1, a1, #1024
	mov sp, a1
	cps #0x17  /* MODE_ABRT */
	add a1, a1, #1024
	mov sp, a1
	cps #0x1B  /* MODE_UNDF */
	add a1, a1, #1024
	mov sp, a1
	cps #0x12  /* MODE_IRQ */
	add a1, a1, #1024
	add a1, a1, #4096
	mov sp, a1
	cps #0x13  /* MODE_SVC */
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
 */
.global start_process_asm
start_process_asm:
	/* reset the stack pointer cause we don't need it any more */
	ldr sp, =stack_end
	cps #0x10
	mov pc, a1

/*
 * Return from syscall or IRQ.
 *
 * This routine is a general-purpose return mechanism. It allows us to return
 * from IRQ, SVC, or SYS mode into SVC, SYS, or USR, depending on whatever
 * context was handed to us in a3. a1 and a2 are used to optionally return a
 * value (in the case of a syscall).
 *
 * This may be called as a function by C code to return early from an exception,
 * but in both SWI and IRQ, it will naturally be executed in the return path.
 *
 * a1: Return value placed in register a1 on return, whenever a2 is truthy
 * a2: Do we use the return value? If not, we replace the context value of a1
 * a3: Pointer to the context we are restoring
 */
.global return_from_exception
return_from_exception:
	/* Use a3 as our new stack, for popping items out of the context */
	mov sp, a3

	/*
	 * We must disable interrupts here because "system mode" is reserved for
	 * kernel threads. If we are interrupted while in system mode, the IRQ
	 * handler might assume we're in kthread process context, and decide
	 * that it's safe to attempt to context switch us out - which is not a
	 * good idea!
	 */
	cpsid i

	/*
	 * We need to restore the SP and LR to the mode we'll return to. This is
	 * a bit tricky for two reasons:
	 * (1) We could return to SVC, SYS, or USR modes, depending on what we
	 *     interrupted.
	 * (2) We may currently be in SVC, SYS, or IRQ mode.
	 *
	 * As a result, we can't assume that the destination mode's registers
	 * are banked. When we need to do SVC->SVC or SYS->SYS, we do a
	 * different approach in return_to_same_mode.
	 *
	 * Otherwise, we'll switch to the appropriate mode, stick the SP / LR
	 * into the banked register, and continue onward.
	 */
	CURMODE .req v1
	DSTMODE .req v2
	SPRESTO .req v3
	LRRESTO .req v4
	mrs CURMODE, cpsr
	and CURMODE, CURMODE, #0x1F
	ldr DSTMODE, [sp, #64]
	and DSTMODE, DSTMODE, #0x1F
	pop {SPRESTO, LRRESTO}

	cmp CURMODE, DSTMODE
	beq return_to_same_mode

	cmp DSTMODE, #0x13 /* SVC */
	bne 1f
	cps 0x13 /* SVC */
	b 2f
	1: /* if destination is not SVC, then we should restore to SYS */
	cps 0x1F /* SYS */

	2: /* do the restore */
	mov sp, SPRESTO
	mov lr, LRRESTO

	cmp CURMODE, #0x13
	bne 1f
	/* if we started in SVC, return to SVC */
	cps 0x13
	b 2f
	1: /* otherwise return to IRQ */
	cps 0x12

	2:
	cmp a2, #0
	popeq {a1}  /* when a2==0, restore the stored value of a1 */
	popne {a3}  /* when a2!=0, use current value of a1 */
	pop {a2-a4,r12}
	pop {v1-v8}
	rfefd sp!

return_to_same_mode:
	mov lr, LRRESTO
	mov a3, sp
	mov sp, SPRESTO

	/* transfer everything from given context in a3 to stack */
	add a4, a3, #60
	1:
	add a4, a4, #-4
	ldr v1, [a4]
	push {v1}
	cmp a4, a3
	bne 1b

	cmp a2, #0
	popeq {a1}  /* when a2==0, restore the stored value of a1 */
	popne {a3}  /* when a2!=0, use current value of a1 */
	pop {a2-a4,r12}
	pop {v1-v8}
	rfefd sp!

/*
 * Block the current process (executing in SVC mode) until something marks it as
 * ready again. Only do this if you have made arrangements to be awoken by
 * something... otherwise it's a great way to find yourself in a permanent
 * sleep.
 *
 * a1: pointer to current->context, where we will store enough return state to
 *     continue execution when restored by "return_from_exception"
 */
.global block
block:

	/* Put SPSR and return address into first two bytes */
	add a1, a1, #60
	mrs a2, cpsr
	str a2, [a1, #4]
	adr a2, _block_awaken
	str a2, [a1]

	/* Store remaining items as if we were dumping to stack */
	stmfd a1!, {v1-v8}
	stmfd a1!, {a2-a4,r12}
	mov a2, #0 /* a dummy value for a1 */
	stmfd a1!, {a2}
	stmfd a1!, {sp, lr}

	b schedule /* never returns */

_block_awaken:
	/* Oh hello, we have been awoken! All af that context we stored above is
	 * now replaced, so we can just return! */
	mov pc, lr
