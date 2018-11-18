/*
 * Main program
 */
.text

.global main
main:
	ldr r0, =hello_world
	bl puts
	mov pc, lr

.data
hello_world:
.asciz "Hello, world!\n"
