.text

.global undefined_impl
undefined_impl: sub pc, pc, #8

.global swi_impl
swi_impl:
	stmfd sp, {r0-r12}
	srsfd #0x13 /* supervisor */
	bl swi
	nop
	sub pc, pc, #8

.global prefetch_abort_impl
prefetch_abort_impl:
	stmfd sp, {r0-r12}
	srsfd #0x17 /* abort */
	bl prefetch_abort
	nop
	sub pc, pc, #8

.global irq_impl
irq_impl:
	stmfd sp, {r0-r12}
	srsfd #0x12
	bl irq
	nop
	sub pc, pc, #8

.global fiq_impl
fiq_impl:
	stmfd sp, {r0-r12}
	srsfd #0x11
	bl fiq
	nop
	sub pc, pc, #8

.global data_abort_impl
data_abort_impl:
	stmfd sp, {r0-r12}
	srsfd #0x17 /* abort */
	bl data_abort
	nop
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
