.text

.global undefined_impl
undefined_impl: sub pc, pc, #8

/*
 * Handle SWI. To do this we must store the process context and then dispatch
 * the correct function based on the system call number.
 *
 * When a process makes a system call, they expect that their temporary
 * registers may be clobbered. As a result, registers a1-a4 and r12 are not
 * stored or restored in the SWI handler. This way, the return value from a
 * system call C function (generally stored in a1) is preserved.
 *
 * Unfortunately, this means that this process context is different from the one
 * produced by the IRQ handler, and so we can't context switch directly between
 * IRQ and SWI handlers.
 */
.global swi_impl
swi_impl:
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
	 */
	cpsie i

	/* Look at the SWI instruction and get the interrupt number. */
	ldr v1, [lr, #-4]
	bic v1, v1, #0xFF000000

	adr lr, _swi_ret           /* set our return address */
	cmp v1, #3                 /* compare to max syscall number */
	movhi a1, v1               /* if higher, go to generic swi() with */
	bhi swi                    /* syscall number as arg */
	add pc, pc, v1, lsl #2     /* branch to pc + interrupt number * 4 */
	nop

	/* SYSTEM CALL TABLE */
	/* 0 */ b sys_relinquish
	/* 1 */ b sys_display
	/* 2 */ b sys_exit
	/* 3 */ b sys_getchar
	/* END. Please update max syscall number above. */

_swi_ret:
	mov a2, #1 /* please use return value */
	b return_from_exception

.global prefetch_abort_impl
prefetch_abort_impl:
	srsfd sp!, #0x17 /* abort */
	push {r0-r12}
	bl prefetch_abort
	nop  /* infinite loop since it broke */
	sub pc, pc, #8

/**
 * Handle IRQ.
 *
 * An IRQ may occur while in any processor mode -- user or kernel. The
 * assumption here (which is incorrect, to be sure), is that an IRQ will be
 * handled briefly, and then the original execution will return, with no
 * possibility of the process being context switched out.
 */
.global irq_impl
irq_impl:
        /* Dump LR and SPSR to IRQ-mode stack. */
	srsfd sp!, #0x12 /* irq */

	/* Save registers in standard order */
	push {v1-v8}
	push {a2-a4,r12}
	push {a1}

	/*
	 * Save SP_usr and LR_usr. If we're interrupting the kernel (e.g.  a
	 * syscall) then this probably won't be used. But if we're interrupting
	 * a user process, we may context switch it out.
	 */
	cps #0x1F  /* system */
	mov v1, sp
	mov v2, lr
	cps #0x12  /* irq */
	push {v1, v2}

	/* Branch into the C IRQ handler. */
	bl irq
	mov a2, #0  /* please don't use return value */
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
	cps #0x12  /* MODE_IRQ */
	add a1, a1, #1024
	mov sp, a1
	cps #0x17  /* MODE_ABRT */
	add a1, a1, #1024
	mov sp, a1
	cps #0x1B  /* MODE_UNDF */
	add a1, a1, #1024
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
 * This routine lets us return the value of a1 (depending on the value of a2) to
 * the previously running code, or preserve the stored value of a1. Which is
 * chosen depends on the value of a2. If we're returning from an interrupt, in
 * most cases we will use the stored value. However, it is feasible that during
 * an interrupt we could context switch a process which will return from a
 * syscall, in which case we would use the returned value. For returning from
 * SWI, we will always use the returned value.
 *
 * This may be called as a function by C code to return early from an exception,
 * but in both SWI and IRQ, it will naturally be executed in the return path.
 *
 * a1: Return value placed in register a1 on return, whenever a2 is truthy
 * a2: Do we use the return value? If not, we replace the context value of a1
 */
.global return_from_exception
return_from_exception:
	/* Reset sp back to the top of stack. */
	ldr a3, =0xFFFFFC00
	and sp, sp, a3
	add sp, sp, #0x400
	/* And move it back 68 bytes, the context size */
	add sp, sp, #-68  /* context size: 17 words = 68 bytes */

	/* Restore SP_usr and LR_usr */
	mrs a3, cpsr
	and a3, a3, #0x1F
	mov a4, #0x13
	pop {v1, v2}
	cps #0x1F  /* system */
	mov sp, v1
	mov lr, v2
	cmp a3, a4
	bne _back_to_irq
	cps #0x13  /* svc */
	b _back
_back_to_irq:
	cps #0x12  /* irq */
_back:
	mov a3, #0
	cmp a2, a3
	popeq {a1}  /* when a2==0, restore the stored value of a1 */
	popne {a3}  /* when a2!=0, use current value of a1 */
	pop {a2-a4,r12}
	pop {v1-v8}
	rfefd sp!
