SECTIONS {

	mbr_start = 0x8000;

	. = mbr_start;
	.text.entry : { KEEP(*(.text.entry)) }
	.text.init  : { KEEP(*(.text.init)) }
	.data.init  : { KEEP(*(.data.init)) }

	. = mbr_start + 0x1BE;
	.partition_table : {
		partition_table_entries = .;
		LONG(0)
		LONG(0)
		LONG(0)
		LONG(0)
	}

	. = mbr_start + 0x1FE;
	.mbr_signature : {
		BYTE(0x55)
		BYTE(0xAA)
	}

	ss_start = mbr_start + 0x200;

	.text : { *(.text) *(.text.*) }
	.data : { *(.data) *(.data.*) *(.rodata) *(.rodata.*) }
	ss_bss_start = .;
	.bss : { *(.bss) *(.bss.*) *(COMMON) }
	ss_bss_end = .;

	ss_end = .;

	/DISCARD/ : { *(.note.gnu.*) }

}
