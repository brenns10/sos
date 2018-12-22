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

.global _data_abort
_data_abort:
	b _data_abort

/*
 * Change into each mode, and set stack to be 1024 bytes of the page pointed by
 * a1.
 */
.global setup_stacks
setup_stacks:
	cps #0x11
	add a1, a1, #1024
	cps #0x12
	add a1, a1, #1024
	cps #0x17
	add a1, a1, #1024
	cps #0x1B
	add a1, a1, #1024
	cps #0x13
	mov pc, lr
