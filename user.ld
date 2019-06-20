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
	. = 0x40000000;
	code_start = .;
	.startup . : {
		user/startup.o(*)
	}
	.text . : {
		*(.text)
	}
	code_end = .;

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
	. = ALIGN(8);
	stack_start = .;
	. = . + 0x1000; /* 4kB stack memory */
	stack_end = .;
}
