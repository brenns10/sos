.data
message: .asciz "Data abort!\n"
.text

.global _undefined_impl
_undefined_impl: nop

.global _swi_impl
_swi_impl: nop

.global _prefetch_abort_impl
_prefetch_abort_impl: nop

.global _irq_impl
_irq_impl: nop

.global _fiq_impl
_fiq_impl: nop

.global _data_abort_impl
_data_abort_impl:
	stmfd sp, {r0-r12}
	srsfd #0x17
	ldr pc, =_data_abort_trampoline
_data_abort_trampoline:
	ldr a1, =message
	bl puts
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
