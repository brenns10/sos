# Startup conditions for userspace programs:
# * The stack is not configured, do it yourself
# * Execution starts at PC=0x40000000, which normally is the _start handler
.section text
.global _start
_start:
	ldr sp, =stack_end
	bl main
	swi #2
