.section rawdata

.global process_salutations_start
.global process_salutations_end

process_salutations_start:
	.incbin "user/salutations.bin"
	.align 4
process_salutations_end:
	nop

.global process_hello_start
.global process_hello_end

process_hello_start:
	.incbin "user/hello.bin"
	.align 4
process_hello_end:
	nop
