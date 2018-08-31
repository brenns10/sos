ENTRY(main)
SECTIONS {
	.init 0x0 : {
		*(.init)
	}
}
