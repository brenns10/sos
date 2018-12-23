.data
message: .asciz "Data abort!\n"
.text

.global undefined_impl
undefined_impl: nop

.global swi_impl
swi_impl: nop

.global prefetch_abort_impl
prefetch_abort_impl: nop

.global irq_impl
irq_impl: nop

.global fiq_impl
fiq_impl: nop

.global data_abort_impl
data_abort_impl:
	stmfd sp, {r0-r12}
	srsfd #0x17
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
