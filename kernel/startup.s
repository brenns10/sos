/*
 * startup.s: Kernel entry point. Does low-level MMU initialization and
 * branching into the correct memory location. Sets up stackc. Calls C main().
 *
 * This stub requires linker symbols:
 *
 *   - code_start: the VIRTUAL address where _start should end up mapped to
 *   - first_level_table: The VIRTUAL address where the first level page table
 *     should end up. This is directly after code, data, and stack.
 *
 * The physical address at which this code is loaded should not matter, provided
 * it is aligned to a 16KB (0x4000) boundary.
 *
 * No routine in this file should make use of the stack, all arguments should be
 * passed in registers. This speeds things up and prevents us from having to
 * setup the stack until after MMU is enabled.
 */
.text

/*
 * Hey look, it's an interrupt vector! Have some commentary about it:
 *
 * Code from startup.s resides at the beginning of the code section, on a page
 * boundary. We map this page to address 0x00 (in addition to the normal address
 * for this code) so that it can also serve as the interrupt vector.
 *
 * Code from entry.s resides directly after. It contains the _impls for these
 * branches, which are the Interrupt Service Routines. The linker will ensure
 * our ISRs reside completely within this first page.
 *
 * We should never really use the "reset" vector. I suspect this code might
 * break if it was used diretly.
 */
.globl _start
_start: b start_impl
_undefined: ldr pc, =undefined_impl
_swi: ldr pc, =swi_impl
_prefetch_abort: ldr pc, =prefetch_abort_impl
_data_abort: ldr pc, =data_abort_impl
_undefined_interrupt: nop
_irq: ldr pc, =irq_impl
_fiq: ldr pc, =fiq_impl

start_impl:
	/*
	 * Step 1: Figure out where the translation table base belongs and
	 * initialize it.
	 *
	 * Symbols loaded via the adr instruction are pc-relative, and thus are
	 * nearby (physical addresses).
	 */
	adr a1, _start

	/*
	 * Linker symbols loaded via ldr are virtual addresses, and may be far
	 * away from here.
	 *
	 * Compute the physical address of the first level table by computing an
	 * offset from the code start, and adding that to the physical address
	 * of the code start.
	 */
	ldr a2, =code_start
	ldr a3, =first_level_table
	sub a3, a3, a2
	add a1, a1, a3

	/* align to 14 bits (0x4000) for page table base */
	sub a1, a1, #1
	lsr a1, a1, #14
	add a1, a1, #1
	lsl a1, a1, #14

	mov a2, #0

	bl init_first_level

	/*
	 * Step 2: Identity map the currently executing code.
	 * We go from _start to the end of the page tables.
	 * a1 base, a2 + a3 are _start, a4 is length, v4 is flags
	 */
	adr a2, _start
	mov a3, a2
	ldr a4, =#0x00404000
	add a4, a1, a4
	sub a4, a4, a3
	mov v4, #0x10 /* PRW_UNA */
	bl map_pages

	/*
	 * Step 3: Map the currently executing code to its new address.
	 * a1 base, a2 is code_start, a3 is _start, a4 is len, v4 is flags
	 */
	ldr a2, =code_start
	adr a3, _start
	ldr a4, =#0x00404000
	add a4, a1, a4
	sub a4, a4, a3
	mov v4, #0x10 /* PRW_UNA */
	bl map_pages

	/*
	 * Step 3.5: Identity map 0x90000000 in, for UART.
	 */
	mov a2, #0x09000000
	mov a3, #0x09000000
	mov a4, #0x1000
	mov v4, #0x10 /* PRW_UNA */
	bl map_pages

	/*
	 * Step 4: Enable the MMU!
	 */
	mcr p15, 0, a1, c2, c0, 0  /* Set TTBR0 to our base */

	mov a1, #0x1
	mcr p15, 0, a1, c3, c0, 0 /* access control for domain 0 */

	mrc p15, 0, a1, c1, c0, 0
	ldr a2, =0x00800001
	orr a1, a1, a2
	mcr p15, 0, a1, c1, c0, 0 /* set vmsa6 bit, set mmu enable */

	adr a1, _start /* put physical address of start as first arg to main */
	/*
	 * Step 4.5: Branch above the kernel/user space split. We use ldr to
	 * ensure that we are actually loading the linker-provided address for
	 * trampoline, which should be the virtual address.
	 *
	 * This ensures that when we branch and link, the return address on the
	 * stack will be in kernel-space and not user space.
	 */
	ldr pc, =_trampoline
.globl _trampoline
_trampoline:

	/*
	 * Step 5: Branch into C code! (after the stack pointer is set)
	 */
	ldr sp, =stack_end
	bl main

	/*
	 * Step 6: Infinite loop at end.
	 */
	sub pc, pc, #8

/**
 * Initialize first level page descriptor table, to point to the second level
 * descriptors directly after. They will all be set to unmapped.
 *
 * a1: base of page descriptor table (preserved)
 * a2: any flags to add to the descriptor
 */
init_first_level:
	base       .req a1
	flags      .req a2
	descriptor .req a3

	/* Descriptor will start pointing at the end of the first level, and
	 * should include any flags given to us. */
	add descriptor, base, #0x4000
	orr  descriptor, descriptor, flags

	/* Loop termination condition */
	.unreq flags
	end .req a2
	add end, base, #0x4000

	/* Now let's actually setup the table */
	_init_first_level_loop:
		str descriptor, [base], #4
		add descriptor, descriptor, #1024
		cmp base, end
		blo _init_first_level_loop

	sub base, base, #0x4000
	mov pc, lr
	.unreq base
	.unreq end
	.unreq descriptor

/*
 * A subroutine that will map address `virt` to `phys` in the first/second level
 * page tables using a coarse first level mapping, then a `small page` second
 * level mapping. The mapping goes for `len` bytes, where `len` must be a
 * multiple of 4KB (0x1000). The `flags` are included in the mapping.
 *
 * Before this subroutine is called, `base` must be initialized (e.g. with
 * init_first_level) to a table containing pointers to second-level tables, but
 * all marked as unmapped.
 *
 * This subroutine does not respect calling conventions at all - our aim here is
 * to set up the MMU as early as possible, without having to deal with things
 * like setting up the stack. As a result, we do all this with out touching the
 * stack, and arguments are all passed as registers.
 *
 * a1 - base (preserved)
 * a2 - virt (clobbered)
 * a3 - phys (clobbered)
 * a4 - len  (clobbered)
 * v4 - flags (preserved)
 * CLOBBERS v1, v2, v3, v5
 * No return value.
 */
map_pages:
	base  .req a1
	virt  .req a2
	phys  .req a3
	len   .req a4
	flags .req v4

	map_pages_loop:

		cmp len, #0
		movle pc, lr

		/* load up the first level descriptor */
		offset .req v1
		fld .req v2
		lsr offset, virt, #20
		lsl offset, offset, #2
		ldr fld, [base, offset]

		/* check whether this descriptor has been mapped yet*/
		type .req v3
		and type, fld, #3
		cmp type, #1
		beq _map_pages_after_init
		.unreq type

		/* Initialize the second level table! Start by marking the first
		 * level descriptor as mapped. */
		and fld, fld, #0xFFFFFFFC
		orr fld, fld, #0x01
		str fld, [base, offset]

		/* Then zero out the second level table */
		.unreq fld
		second .req v2 /* fld is now the address of second table */
		end .req v3

		ldr v5, =#0xFFFFFC00
		and second, second, v5
		add end, second, #1024
		zero .req v5
		mov zero, #0
		_second_level_loop:
			str zero, [second], #4
			cmp second, end
			blo _second_level_loop

		/* Return second to its original value */
		sub second, second, #1024
		.unreq end
		.unreq zero

	_map_pages_after_init:
		ldr v5, =#0xFFFFFC00
		and second, second, v5

		/* form the offset into the second level table */
		lsr offset, virt, #10
		and offset, offset, #0x3FC

		/* and create the entry we want to store there */
		sld .req v3
		mov sld, phys
		orr sld, sld, #2
		orr sld, sld, flags

		/* now store the second level descriptor */
		str sld, [second, offset]

		/* and now we can increment / decrement everything before looping */
		add virt, virt, #0x1000
		add phys, phys, #0x1000
		sub len, len, #0x1000
		b map_pages_loop

	.unreq base
	.unreq virt
	.unreq phys
	.unreq len
	.unreq flags
	.unreq sld
	.unreq second
	.unreq offset
