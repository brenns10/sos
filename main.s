/*
 * Main program
 */
.text

.global main
main:
	ldr r0, =hello_world
	push {lr}
	bl puts
	pop {pc}

.data
hello_world:
.asciz "Hello, world!\n"
