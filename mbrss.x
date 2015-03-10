SECTIONS {

	mbr_start = 0x8000;

	. = mbr_start;
	.text.mbr : { KEEP(*(.text.mbr)) }
	.data.mbr : { KEEP(*(.data.mbr)) }

	. = mbr_start + 0x1BE;
	.partition_table : {
		LONG(0)
	}

	. = mbr_start + 0x1FE;
	.mbr_signature : {
		BYTE(0x55)
		BYTE(0xAA)
	}

	ss_start = mbr_start + 0x200;

	. = ss_start;
	.ss_stub : { KEEP(*(.text.ss)) }

	ss_hdr_size = ss_start + 16;
	ss_hdr_szinv = ss_start + 20;

	. = ss_hdr_size;
	.ss_meta : {
		LONG(ss_end - mbr_start)
		LONG(~(ss_end - mbr_start))
	}

	. = ss_start + 32;
	.text : { *(.text) *(.text.*) }
	.data : { *(.data) *(.data.*) *(.rodata) *(.rodata.*) }
	ss_bss_start = .;
	.bss : { *(.bss) *(.bss.*) *(COMMON) }
	ss_bss_end = .;

	ss_end = .;

	. = ALIGN(16);
	e820_entries = .;

	/DISCARD/ : { *(.note.gnu.*) }

}
