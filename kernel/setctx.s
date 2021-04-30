.equ MODE_USER, 0x10
.equ MODE_FIQ,  0x11
.equ MODE_IRQ,  0x12
.equ MODE_SVC,  0x13
.equ MODE_ABRT, 0x17
.equ MODE_HYP,  0x1A
.equ MODE_UNDF, 0x1B
.equ MODE_SYS,  0x1F
.equ MODE_MASK, 0x1F

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
	mov a2, sp

	/* If currently in SYS mode, move into SVC mode so we know we can safely
	 * return without touching a user-mode stack. */
	cmp CURMODE, #MODE_SYS
	bne 1f
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
