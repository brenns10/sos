.equ MODE_USER, 0x10
.equ MODE_FIQ,  0x11
.equ MODE_IRQ,  0x12
.equ MODE_SVC,  0x13
.equ MODE_ABRT, 0x17
.equ MODE_UNDF, 0x1B
.equ MODE_SYS,  0x1F
.equ MODE_MASK, 0x1F
.text

.section .nopreempt
/*
 * setctx(): store CPU context into memory. Return (in a1) 0 on the first call,
 * some non-zero value of a1 when returning via a call to resctx().
 *
 * As in any function call, callee-save registers are not saved in any
 * meaningful way.
 *
 * This function CAN ONLY store the context of SVC or SYS mode (kernel)
 * execution contexts. This means that kthreads can use it, and so can system
 * calls operating on behalf of userspace threads. Interrupts may not use it,
 * nor may they use the opposite function resctx() to restore a context.
 */
.global setctx
setctx:
	add a1, #68
	/* Put CPSR and return address into first two words */
	mrs a2, cpsr
	stmfd a1!, {a2}
	adr a2, 1f
	stmfd a1!, {a2}

	/* Store remaining items as if we were dumping to stack */
	stmfd a1!, {v1-v8}
	stmfd a1!, {a2-a4,r12}
	mov a2, #1 /* a dummy value for a1 */
	stmfd a1!, {a2}
	stmfd a1!, {sp, lr}

	/* And return 0 back */
	mov a1, #0
	mov pc, lr

	1:
		mov pc, lr

/*
 * resctx(): restore CPU context and resume executing wherever we left off.
 * a1: an argument to return from the second return of setctx(). When a1 is 0,
 * this signals that we should use the value of a1 stored in a1 (i.e. a complete
 * context restore) since using 0 would confuse any caller of setctx().
 * a2: a pointer to the context we return to
 *
 * We may resume from SVC or SYS mode, and we may resume to SVC, SYS, or USR
 * mode. WE MAY NOT RESUME FROM IRQ, please simply return from the IRQ handler.
 */
.global resctx
resctx:
	/* Use the second argument (context) as "stack" to pop items out */
	mov sp, a2

	/*
	 * We must disable interrupts here because "system mode" is reserved for
	 * kernel threads. If we are interrupted while in system mode, the IRQ
	 * handler might assume we're in kthread process context, and decide
	 * that it's safe to attempt to context switch us out - which is not a
	 * good idea!
	 */
	cpsid i

	CURMODE .req v1
	DSTMODE .req v2
	SPRESTO .req v3
	LRRESTO .req v4
	mrs CURMODE, cpsr
	and CURMODE, CURMODE, #MODE_MASK
	ldr DSTMODE, [sp, #64]
	and DSTMODE, DSTMODE, #MODE_MASK
	pop {SPRESTO, LRRESTO}

	/* If currently in SYS mode, move into SVC mode so we know we can safely
	 * return without touching a user-mode stack. */
	cmp CURMODE, #MODE_SYS
	bne 1f
		mov a2, sp
		cps #MODE_SVC
		mov sp, a2

	1:
	/* If we are returning from SVC->SVC, we'll need to do a special-case
	 * form where we transfer the context onto the existing stack and then
	 * restore it from there. */
	cmp DSTMODE, #MODE_SVC
	beq return_to_svc

	/* At this point we know we are in SVC returning to SYS/USR */
	cps #MODE_SYS
	mov sp, SPRESTO
	mov lr, LRRESTO
	cps #MODE_SVC
	cmp a1, #0
	popeq {a1}  /* when a1==0, restore the stored value of a1 */
	popne {a3}  /* when a1!=0, use current value of a1 */
	pop {a2-a4,r12}
	pop {v1-v8}
	rfefd sp!

return_to_svc:
	mov lr, LRRESTO
	mov sp, SPRESTO

	/* transfer everything from given context in a3 to stack */
	add a3, a2, #60
	1:
	add a3, a3, #-4
	ldr v1, [a3]
	push {v1}
	cmp a3, a2
	bne 1b

	cmp a1, #0
	popeq {a1}  /* when a1==0, restore the stored value of a1 */
	popne {a3}  /* when a1!=0, use current value of a1 */
	pop {a2-a4,r12}
	pop {v1-v8}
	rfefd sp!

.section .text

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
	srsfd sp!, #MODE_SVC
	push {v1-v8}
	push {a2-a4,r12} /* not guaranteed to be saved but we do */
	push {a1} /* will be clobbered by retval but stored anyway */

	/* Save SP_usr and LR_usr */
	cps #MODE_SYS
	mov v1, sp
	mov v2, lr
	cps #MODE_SVC
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
	cmp v1, #10                /* compare to max syscall number */
	movhi a1, v1               /* if higher, go to generic swi() with */
	bhi sys_unknown            /* syscall number as arg */
	add pc, pc, v1, lsl #2     /* branch to pc + interrupt number * 4 */
	nop

	/* SYSTEM CALL TABLE */
	/*  0 */ b sys_relinquish
	/*  1 */ b sys_display
	/*  2 */ b sys_exit
	/*  3 */ b sys_getchar
	/*  4 */ b sys_runproc
	/*  5 */ b sys_getpid
	/*  6 */ b sys_socket
	/*  7 */ b sys_bind
	/*  8 */ b sys_connect
	/*  9 */ b sys_send
	/* 10 */ b sys_recv
	/* END. Please update max syscall number above. */
_swi_ret:
	pop {v1, v2}
	cps #MODE_SYS
	mov sp, v1
	mov lr, v2
	cps #MODE_SVC
	pop {a2} /* discard a1 since we're returning */
	pop {a2-a4,r12}
	pop {v1-v8}
	rfefd sp!

.global undefined_impl
undefined_impl:
	srsfd sp!, #MODE_UNDF
	push {v1-v8}
	push {a2-a4,r12}
	push {a1}
	/* Push dummy values for SP and LR of interrupted mode. */
	mov a1, #0
	mov a2, #0
	push {a1, a2}
	mov a1, sp
	bl undefined
	nop
	sub pc, pc, #8

.global prefetch_abort_impl
prefetch_abort_impl:
	srsfd sp!, #MODE_ABRT
	push {v1-v8}
	push {a2-a4,r12}
	push {a1}
	/* Push dummy values for SP and LR of interrupted mode. */
	mov a1, #0
	mov a2, #0
	push {a1, a2}
	mov a1, sp
	bl prefetch_abort
	nop
	sub pc, pc, #8

.global fiq_impl
fiq_impl:
	srsfd sp!, #MODE_FIQ
	push {v1-v8}
	push {a2-a4,r12}
	push {a1}
	/* Push dummy values for SP and LR of interrupted mode. */
	mov a1, #0
	mov a2, #0
	push {a1, a2}
	mov a1, sp
	bl fiq
	nop
	sub pc, pc, #8

.global data_abort_impl
data_abort_impl:
	srsfd sp!, #MODE_ABRT
	push {v1-v8}
	push {a2-a4,r12}
	push {a1}
	/* Push dummy values for SP and LR of interrupted mode. */
	mov a1, #0
	mov a2, #0
	push {a1, a2}
	mov a1, sp
	bl data_abort
	nop
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
	/* Load IRQ-mode stack */
	ldr sp, =irq_stack
	ldr sp, [sp]

	/*
	 * The lr points at the interrupted instruction. To resume, reset it
	 * back by one instruction.
	 * NOTE: assumes that we don't have Thumb instructions
	 */
	add lr, lr, #-4

        /* Dump LR and SPSR to IRQ stack */
	srsfd sp!, #MODE_IRQ

	/* Save registers in standard order */
	push {v1-v8}
	push {a2-a4,r12}
	push {a1}

	/*
	 * Save SP and LR of interrupted mode.
	 *
	 * We may interrupt SVC, SYS, or USR mode. To retrieve the correct SP
	 * and LR, we need to switch to SVC in the first case, or SYS for the
	 * remaining two.
	 */
	mrs v1, spsr
	and v1, v1, #MODE_MASK
	cmp v1, #MODE_SVC
	bne 1f
		cps #MODE_SVC
		b 2f
	1:
		cps #MODE_SYS
	2:
	mov v1, sp
	mov v2, lr
	/* Now return to IRQ mode and push those registers to the stack */
	cps #MODE_IRQ
	push {v1, v2}

	/* Call the C IRQ handler. */
	mov a1, sp
	bl irq

	/* Now restore SP and LR of interrupted mode (may be different now). */
	ldr v1, [sp, #64]  /* grab saved spsr */
	pop {v2, v3}     /* pop saved sp / lr */
	and v1, v1, #MODE_MASK
	cmp v1, #MODE_SVC /* SVC */
	bne 1f
		cps #MODE_SVC
		b 2f
	1:
		cps #MODE_SYS /* retrieve from SYS or USR modes */
	2:
	mov sp, v2
	mov lr, v3
	cps #MODE_IRQ

	pop {a1}
	pop {a2-a4,r12}
	pop {v1-v8}
	rfefd sp!

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
