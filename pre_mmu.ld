ENTRY(_start)
SECTIONS {
	. = 0x40010000;
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
	. = ALIGN(8);
	stack_start = .;
	. = . + 0x1000; /* 4kB stack memory */
	stack_end = .;

	/* Unused memory here */
	. = ALIGN(0x4000);
	unused_start = .;
	dynamic_start = .;
	first_level_table = .;
}
