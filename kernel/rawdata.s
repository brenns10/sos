.section .data

.global process_salutations_start
.type process_salutations_start,object
.global process_salutations_end
.type process_salutations_end,object

	.align 4
process_salutations_start:
	.incbin "user/salutations.bin"
	.align 4
process_salutations_end:
	nop

.global process_hello_start
.type process_hello_start,object
.global process_hello_end
.type process_hello_end,object

	.align 4
process_hello_start:
	.incbin "user/hello.bin"
	.align 4
process_hello_end:
	nop

.global process_ush_start
.type process_ush_start,object
.global process_ush_end
.type process_ush_end,object

	.align 4
process_ush_start:
	.incbin "user/ush.bin"
	.align 4
process_ush_end:
	nop
