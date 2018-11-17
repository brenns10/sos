/*
 * Print to serial on PL011 UART.
 */
.text

.globl _start
_start:
	/* Set stack pointer to top of stack, grows downward.
	 * Top of stack is defined in the link script.
	 */
	ldr sp, =stack_top

	mov r0, #'A'
	bl print
	ldr r0, =hello_world
	bl print_string
	mov r0, #'Z'
	bl print
loop:
	b loop

/* Prints the nul-terminated string pointed by r0 */
print_string:
	push {r4, lr}
	mov r4, r0
_print_string_loopldr:
	ldrb r0, [r4]
	cmp r0, #0
	beq _print_string_return
	bl print
	add r4, #1
	b _print_string_loopldr
_print_string_return:
	pop {r4, pc}

/* Prints the byte in r0 */
.equ UART_BASE, 0x09000000
.equ FR, 0x018

print:
	mov r1, #0x020     /* bit 5 of flag register = TX FIFO Full  */
	mov r2, #UART_BASE
_print_wait:
	ldr r3, [r2, #FR]
	tst r3, r1         /* wait until TX FIFO is not full */
	bne _print_wait
	str r0, [r2]       /* DR has offset 0 */
	mov pc, lr

.data
hello_world: .asciz "Hello\n"
