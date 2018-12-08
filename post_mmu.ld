ENTRY(_start)
SECTIONS {
	. = 0xC0000000;
	code_start = .;
	.startup . : {
		startup.o(.text)
	}
	.text . : {
		*(.text)
	}

	/* data on separate page to allow enforcing execute never */
	. = ALIGN(0x1000);
	code_end = .;
	data_start = .;
	.data . : {
		*(.data)
	}

	/* Kernel mode stack */
	. = ALIGN(8);
	data_end = .;
	stack_start = .;
	. = . + 0x1000; /* 4kB stack memory */
	stack_end = .;

	/* Unused memory here */
	. = ALIGN(0x4000);
	unused_start = .;
	dynamic_start = .;
}
