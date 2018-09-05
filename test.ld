ENTRY(main)
SECTIONS {
	.init 0x40000000 : {
		*(.init)
	}
}
