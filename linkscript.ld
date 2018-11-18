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

	. = ALIGN(8);
	. = . + 0x10000; /* 4kB stack memory */
	stack_top = .;
}
