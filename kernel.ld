/*
 * This file specifies locations in the virtual address space where the final
 * sections will be located.
 *
 * On startup, QEMU loads the code at the physical address, 0x4001000. However,
 * the exact address should not matter. startup.s is position-independent, and
 * maps the code to the addresses specified by the linker symbols defined here.
 */
ENTRY(_start)
SECTIONS {
	. = 0x00000000;
	code_start = .;
	.startup . : {
		kernel/startup.o(*)
	}
	.entry . : {
		kernel/entry.o(*)
	}
	ASSERT(. < code_start + 0x1000, "more than one page of memory for startup + ISR")
	.text . : {
		*(.text)
	}
	code_end = .;

	/* data on separate page to allow enforcing execute never */
	. = ALIGN(0x1000);
	data_start = .;
	.rodata . : {
		*(.rodata)
	}
	.data . : {
		*(.data)
	}
	.bss . : {
		*(.bss)
	}
	data_end = .;

	/* Kernel mode stack */
	. = ALIGN(0x1000);
	stack_start = .;
	. = . + 0x1000; /* 4kB stack memory */
	stack_end = .;

	/* Unused memory here */
	. = ALIGN(0x4000);
	unused_start = .;
	dynamic_start = .;
	first_level_table = .;
}
