ENTRY(kernel_entry)

SECTIONS {

	kernel_start = 0xffffffffc0000000;

	. = kernel_start;

	.text : ALIGN(0x1000) {
		*(.text .text.*)
	}
	.rodata : {
		*(.rodata .rodata.*)
	}
	.data : {
		*(.data .data.*)
	}
	.got : {
		*(.got .got.*)
	}
	.bss : {
		bss_start = .;
		*(.bss .bss.*)
		bss_end = .;
	}

	kernel_end = .;

}
