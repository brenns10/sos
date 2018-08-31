/*
 * Basic memory layout:
 *
 *     0x00000000 - Reset vector
 */
.section .init
.equ UART_BASE, 0x09000000
.equ FR, 0x018

.globl main
main:
	mov sp, #0x1000
	mov r0, #'h'
	bl print
	mov r0, #'i'
	bl print
	mov r0, #' '
	bl print
	mov r0, #'w'
	bl print
	mov r0, #'r'
	bl print
	mov r0, #'l'
	bl print
	mov r0, #'d'
	bl print
loop:
	b loop

print:
	mov r1, #0x020  /* bit 5 */
	mov r2, #UART_BASE
_print_wait:
	ldr r3, [r2, #FR]
	tst r3, r1
	bne _print_wait
	str r0, [r2]
	mov pc, lr
