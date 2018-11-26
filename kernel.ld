ENTRY(_start)
SECTIONS {
	. = 0x40010000;
	.startup . : {
		startup.o(.text)
	}
	.text . : {
		*(.text)
	}
	.data . : {
		*(.data)
	}

	/* Kernel mode stack */
	. = ALIGN(8);
	stack_start = .;
	. = . + 0x1000; /* 4kB stack memory */
	stack_end = .;

	/* Dynamic memory begins here */
	. = ALIGN(0x1000); /* 4kB aligned since dynamic memory is page-by-page */
	dynamic_start = .;
}
