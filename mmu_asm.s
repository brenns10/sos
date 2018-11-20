.text

.global enable_mmu
enable_mmu:
	push {lr}
	/* call C helper for doing heavy lifting of creating page tables */
	ldr r0, =page_table_base
	bl init_page_tables
	
	/* translation table base */
	ldr r0, =page_table_base
	mcr p15, 0, r0, c2, c0, 0

	/* domain 0 should be access controlled */
	mov r0, #0x1
	mcr p15, 0, r0, c3, c0, 0

	/* we're supposed to disable the instruction cache and invalidate it. no
	 * idea how to do that */

	/* i guess just turn on the mmu? */
	mrc p15, 0, r0, c1, c0, 0
	/* xp bit: use vmsa6, and mmu enable */
	ldr r1, =0x00800001
	orr r0, r0, r1
	mcr p15, 0, r0, c1, c0, 0

	pop {pc}
