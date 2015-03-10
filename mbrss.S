#include "x86.h"

.code16

.section .text.mbr
mbr_entry:

	/* disable interrupts */
	cli

	/* ground %ss and set %bp */
	mov %ss, %ax
	shl $4, %ax
	add %ax, %sp
	mov %sp, %bp
	xor %ax, %ax
	mov %ax, %ss

	/* ground %ds and %es */
	mov %ax, %ds
	mov %ax, %es

	/* ground %cs after copying to mbr_start */
	call 6f
	6:
	pop %ax
	mov %cs, %si
	shl $4, %si
	add %ax, %si
	add $(mbr_entry - 6b), %si
	mov $mbr_start, %di
	cmp %si, %di
	je .Lafter_copy
	mov $512, %cx
	rep movsb (%si), (%di)
	ljmp $0, $.Lafter_copy
	.Lafter_copy:

	/* save drive index */
	mov %dl, bios_drive_index

	/* write loading string */
	mov $loading_ss_string, %si
	call write_string_real_mode

	/* read first sector in second stage */
	mov $1, %eax
	xor %edx, %edx
	mov %dx, %es
	mov $ss_start, %di
	call read_sector_real_mode
	jc .Lentry_error

	/* get second stage size */
	mov ss_hdr_size, %ebx
	mov ss_hdr_szinv, %ecx
	not %ecx
	cmp %ecx, %ebx
	jne .Lss_invalid

	/* convert to number of sectors */
	add $511, %ecx
	shr $9, %ecx

	/* read rest of sectors */
	4:
	mov %es, %bx
	add $(512 >> 4), %bx
	mov %bx, %es
	inc %eax
	cmp %ecx, %eax
	je 5f
	call read_sector_real_mode
	jc .Lentry_error
	jmp 4b
	5:

	/* write done string */
	mov $done_string, %si
	call write_string_real_mode

	/* set GDT */
	lgdt initial_gdt_register

	/* exit real mode and jump to second stage */
	pushl $ss_stub
	jmp exit_real_mode_helper

.Lss_invalid:
	/* write invalid string */
	mov $invalid_string, %si
	call write_string_real_mode
	jmp .Lsleep_forever

.Lentry_error:
	/* write error string */
	mov $error_string, %si
	call write_string_real_mode
	jmp .Lsleep_forever

.Lsleep_forever:
	/* sleep forever */
	hlt
	jmp .Lsleep_forever

read_sector_real_mode: /* LBA in %edx:%eax, target in %es:%di, error code in %bl, error status in CF */
	/* prologue */
	push %bp
	push %si
	push %eax
	push %edx
	mov %sp, %bp
	/* setup DAP */
	push %edx
	push %eax
	push %es
	push %di
	pushl $0x00010010
	/* call interrupt */
	mov $0x42, %ah
	mov bios_drive_index, %dl
	mov %sp, %si
	int $0x13
	mov %ah, %bl
	/* epilogue (must preserve flags) */
	mov %bp, %sp
	pop %edx
	pop %eax
	pop %si
	pop %bp
	ret
	
write_character_real_mode: /* charcter to write in %al, clobbers %ax */
	push %bx
	mov $14, %ah
	xor %bx, %bx
	int $0x10
	pop %bx
	ret

write_string_real_mode: /* string to write in %si, clobbers %si */
	push %ax
	1:
	mov (%si), %al
	test %al, %al
	jz 2f
	call write_character_real_mode
	add $1, %si
	jmp 1b
	2:
	pop %ax
	ret

exit_real_mode_helper:
	push %eax
	sidt real_mode_idt
	mov %cr0, %eax
	or $CR0_PE, %eax
	mov %eax, %cr0
	ljmp $(1 << 3), $8f
	8:
	.code32
	mov $(2 << 3), %ax
	mov %ax, %ss
	mov %ax, %ds
	mov %ax, %es
	lidt prot_mode_idt
	pop %eax
	ret
	.code16

.section .data.mbr
.align 16
initial_gdt_table:
#define FLAT_LO(base, limit) (SEG_DESC_BASE_PACK_LO(base) | SEG_DESC_LIMIT_PACK_LO(limit))
#define FLAT_HI(base, limit) (SEG_DESC_BASE_PACK_HI(base) | SEG_DESC_LIMIT_PACK_HI(limit) | SEG_DESC_P)
	/* segment 0: unused */
	.long 0
	.long 0
	/* segment 1: 32-bit code */
	.long FLAT_LO(0, 0xFFFFF)
	.long FLAT_HI(0, 0xFFFFF) | SEG_DESC_G | SEG_DESC_S | SEG_DESC_32 | SEG_DESC_TYPE(0xA)
	/* segment 2: 32-bit data */
	.long FLAT_LO(0, 0xFFFFF)
	.long FLAT_HI(0, 0xFFFFF) | SEG_DESC_G | SEG_DESC_S | SEG_DESC_32 | SEG_DESC_TYPE(0x2)
	/* segment 3: 16-bit code (for real mode entry) */
	.long FLAT_LO(0, 0x0FFFF)
	.long FLAT_HI(0, 0x0FFFF) | SEG_DESC_S | SEG_DESC_TYPE(0xA)
#undef FLAT_LO
#undef FLAT_HI
initial_gdt_register:
	.word initial_gdt_register - initial_gdt_table
	.long initial_gdt_table
real_mode_idt:
	.word 0
	.long 0
prot_mode_idt:
	.word 0
	.long 0
bios_drive_index:
	.byte 0
loading_ss_string:
	.asciz "Loading 2nd stage ... "
error_string:
	.asciz "error!\r\n"
invalid_string:
	.asciz "invalid!\r\n"
done_string:
	.asciz "done!\r\n"

.macro enter_real_mode
	data16 call enter_real_mode_helper
	.code16
.endm

.macro exit_real_mode
	data32 call exit_real_mode_helper
	.code32
.endm

.code32

.section .text.ss
ss_stub:
	/* align stack */
	and $~3, %esp
	/* jump to entry */
	jmp ss_entry

.section .text
enter_real_mode_helper:
	push %eax
	sidt prot_mode_idt
	ljmp $(3 << 3), $8f
	8:
	.code16
	mov %cr0, %eax
	and $~CR0_PE, %eax
	mov %eax, %cr0
	ljmp $0, $9f
	9:
	xor %ax, %ax
	mov %ax, %ss
	mov %ax, %ds
	mov %ax, %es
	lidt real_mode_idt
	pop %eax
	ret
	.code32

/********
void bios_call_service(uint8_t service, bios_register_set *set)
********/
.globl bios_call_service
bios_call_service:
	/* prologue */
	push %ebp
	mov %esp, %ebp
	push %edi
	push %esi
	push %ebx
	/* setup up the stack */
	mov 8(%ebp), %esi // %esi in interrupt no.
	shl $2, %esi
	add (real_mode_idt+2), %esi
	pushl (%esi) // cs:ip of handler
	/* segment registers */
	mov 12(%ebp), %esi
	pushw 26(%esi) // es @ -18(%ebp)
	pushw 24(%esi) // ds @ -20(%ebp)
	/* assign registers */
	mov 12(%ebp), %esi
	mov 0(%esi), %eax
	mov 4(%esi), %ebx
	mov 8(%esi), %ecx
	mov 12(%esi), %edx
	mov 20(%esi), %edi
	mov 16(%esi), %esi
	/* do service call */
	enter_real_mode
	mov -20(%bp), %ds
	mov -18(%bp), %es
	pushf // flags for iret to restore
	lcall *-16(%bp)
	mov %ds, -20(%bp)
	mov %es, -18(%bp)
	exit_real_mode
	/* restore registers */
	push %edi
	mov 12(%ebp), %edi
	mov %eax, 0(%edi)
	mov %ebx, 4(%edi)
	mov %ecx, 8(%edi)
	mov %edx, 12(%edi)
	mov %esi, 16(%edi)
	popl 20(%edi)
	/* restore segments */
	mov 12(%ebp), %edi
	popw 24(%edi) // ds
	popw 26(%edi) // es
	/* clean up the stack */
	add $4, %sp
	/* epilogue */
	pop %ebx
	pop %esi
	pop %edi
	mov %ebp, %esp
	pop %ebp
	ret
