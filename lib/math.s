	.globl __aeabi_idivmod
	.globl __aeabi_idiv
__aeabi_idivmod:
__aeabi_idiv:
	push {lr}
	push {v1, v2}
	mov v1, #0
	cmp a1, #0
	movlt v1, #1
	neglt a1, a1

	mov v2, #0
	cmp a2, #0
	movlt v2, #1
	neglt a2, a2

	bl divide

	eors a3, v1, v2
	negne a1, a1
	cmp v1, #1
	negeq a2, a2
	pop {v1, v2}
	pop {pc}

	.globl divide
	.globl __aeabi_uidivmod
	.globl __aeabi_uidiv
divide:
__aeabi_uidivmod:
__aeabi_uidiv:
	result   .req a1
	dividend .req a2
	divisor  .req a3
	shift    .req a4

	mov divisor, a2
	mov dividend, a1

	clz a1, divisor
	clz a4, dividend
	sub shift, a1, a4
	mov a1, #1
	lsl divisor, divisor, shift
	lsl shift, a1, shift
	mov result, #0

	_divide_loop:
		cmp shift, #0
		moveq pc, lr  /* RETURN */

		cmp dividend, divisor
		blt _divide_shift_right
			sub dividend, dividend, divisor
			add result, result, shift
		_divide_shift_right:
			lsr shift, shift, #1
			lsr divisor, divisor, #1
			b _divide_loop

