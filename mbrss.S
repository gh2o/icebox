#include "x86.h"

.section .text.entry

.code16
mbr_entry_16:

	/* disable interrupts */
	cli

	/* zero control registers */
	xor %eax, %eax
	mov %eax, %cr0
	mov %eax, %cr4

	/* zero %ds and %es */
	mov %ax, %ds
	mov %ax, %es

	/* figure out where we are */
	call 1f
	1: pop %ax
	mov %cs, %bx
	shl $4, %bx
	add %bx, %ax // %ax now contains actual location of 1b

	/* copy it if necessary, zero %cs */
	cmp $1b, %ax
	je .Lno_need_to_copy
	sub $(1b - mbr_entry_16), %ax
	mov %ax, %si
	mov $mbr_entry_16, %di
	mov $512, %cx
	rep movsb
	ljmp $0, $mbr_entry_16
	.Lno_need_to_copy:

	/* zero %ss and set up stack */
	xor %ax, %ax
	mov %ax, %ss
	mov $scratch_sector, %sp

	/* save drive index */
	mov %dl, bios_drive_index

	/* setup GDT */
	lgdt initial_gdt_register

	/* enter protected mode */
	mov $CR0_PE, %eax
	mov %eax, %cr0
	ljmp $(1 << 3), $mbr_entry_32a

real_mode_call_16:
	/* save function address */
	push %ax
	/* disable protected mode */
	mov $0, %eax
	mov %eax, %cr0
	ljmp $0, $2f
	2:
	/* reset segment registers */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	/* call the function */
	pop %ax
	call *%ax
	push %ax
	/* enable protected mode */
	mov $CR0_PE, %eax
	mov %eax, %cr0
	ljmp $(3 << 3), $3f
	3:
	/* reset segment registers */
	mov $(2 << 3), %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	/* done! */
	pop %ax
	lret

read_single_sector_16:
	mov $0x42, %ah
	mov bios_drive_index, %dl
	mov $disk_access_packet, %si
	int $0x13
	ret

write_chracter_16:
	mov $0x0E, %ah
	mov %dl, %al
	mov $0x0F, %bx
	int $0x10
	ret

.code32
mbr_entry_32a:
	/* set up segment registers */
	mov $(2 << 3), %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	/* jump to C code */
	jmp mbr_entry_32b

real_mode_call:
	data16 lcall $(3 << 3), $real_mode_call_16
	ret

read_single_sector:
	/* prologue */
	push %esi
	push %edi
	/* fill in the packet LBA (arg 1) */
	mov 12(%esp), %eax
	mov %eax, disk_access_packet+8
	/* do bios call */
	mov $read_single_sector_16, %ax
	call real_mode_call
	/* copy it into buffer (arg 2) */
	mov $scratch_sector, %esi
	mov 16(%esp), %edi
	mov $512, %ecx
	rep movsb
	/* return error code */
	mov %ah, %al
	/* epilogue */
	pop %edi
	pop %esi
	ret

write_character:
	push %ebx
	mov 8(%esp), %dl
	mov $write_chracter_16, %ax
	call real_mode_call
	pop %ebx
	ret

.section .data.init

scratch_sector = mbr_entry_16 - 512

#define GDT_DESC (SEG_DESC_PACK(0, 0xFFFFF) | SEG_DESC_P)
.align 8
initial_gdt_table_start = initial_gdt_table_actual - 8
initial_gdt_table_actual: /* skip null selector to save space */
	/* segment 1: 32-bit code */
	.quad GDT_DESC | SEG_DESC_G | SEG_DESC_S | SEG_DESC_32 | SEG_DESC_TYPE(0xA)
	/* segment 2: 32-bit data */
	.quad GDT_DESC | SEG_DESC_G | SEG_DESC_S | SEG_DESC_32 | SEG_DESC_TYPE(0x2)
	/* segment 3: 16-bit data */
	.quad GDT_DESC | SEG_DESC_S | SEG_DESC_TYPE(0xA)
initial_gdt_table_end:

.align 4
disk_access_packet:
	.byte 16 // size of packet
	.byte 0  // reserved
	.word 1  // sectors to read
	.long scratch_sector  // sector:offset
	.quad 0  // LBA

.align 2
initial_gdt_register:
	.word initial_gdt_table_end - initial_gdt_table_start
	.long initial_gdt_table_start

.align 1
bios_drive_index:
	.byte 0

.globl read_single_sector
.globl write_character
