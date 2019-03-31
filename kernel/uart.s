/*
 * Print to serial on PL011 UART.
 */
.text

/* Prints the nul-terminated string pointed by r0 */
.global puts
puts:
	push {r4, lr}
	mov r4, r0
_print_string_loopldr:
	ldrb r0, [r4]
	cmp r0, #0
	beq _print_string_return
	bl putc
	add r4, #1
	b _print_string_loopldr
_print_string_return:
	pop {r4, pc}

/* Prints the byte in r0 */
.equ FR, 0x018

.global putc
putc:
	mov r1, #0x020     /* bit 5 of flag register = TX FIFO Full  */
	ldr r2, =uart_base
	ldr r2, [r2]
_print_wait:
	ldr r3, [r2, #FR]
	tst r3, r1         /* wait until TX FIFO is not full */
	bne _print_wait
	str r0, [r2]       /* DR has offset 0 */
	mov pc, lr

.data

.align 4
.global uart_base
uart_base: .word 0x09000000
