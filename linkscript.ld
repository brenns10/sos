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
	. = . + 0x1000; /* 4kB stack memory */
	stack_top = .;

	. = ALIGN(0x4000); /* 16kB aligned for the full page table */
	page_table_base = .;
	. = . + 0x4000;
	page_table_top = .;
}
