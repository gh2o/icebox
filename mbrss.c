#if 0
#define asm __asm__

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <elf.h>
#include "x86.h"

#define VGA_ROWS (25)
#define VGA_COLS (80)
#define VGA_BASE (0xB8000)

typedef struct {
	char character;
	char attributes;
} vga_character_cell;

typedef struct {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint16_t ds;
	uint16_t es;
} bios_register_set;

typedef struct {
	uint64_t base;
	uint64_t length;
	uint32_t type;
	uint32_t present;
} e820_entry;

typedef struct {
	uint8_t part_status;
	uint8_t first_chs[3];
	uint8_t part_type;
	uint8_t last_chs[3];
	uint32_t first_sector;
	uint32_t sector_count;
} partition_entry;

extern void bios_call_service(uint8_t service, bios_register_set *set);

struct vga_row { vga_character_cell cols[VGA_COLS]; };
struct vga_grid { struct vga_row rows[VGA_ROWS]; };
static const union {
	uint32_t base;
	volatile vga_character_cell *direct;
	volatile struct vga_grid *grid;
} vga_frame_buffer = { VGA_BASE };

extern e820_entry e820_entries[];
unsigned int e820_entries_count = 0;

uint16_t vga_cursor;

// page-aligned allocator
unsigned char *pal_alloc_start;
unsigned char *pal_alloc_end;

// VM page table
uint64_t *initial_pml4;

static void clear_bss() {
	extern char ss_bss_start;
	extern char ss_bss_end;
	char *start = &ss_bss_start;
	char *end = &ss_bss_end;
	__builtin_memset(start, 0, end - start);
}

static uint8_t in8(uint16_t reg) {
	uint8_t val;
	asm volatile(
		"in %%dx, %%al\n\t"
		: "=a"(val)
		: "d"(reg));
	return val;
}

static void out8(uint16_t reg, uint8_t val) {
	asm volatile(
		"out %%al, %%dx\n\t"
		:
		: "a"(val), "d"(reg));
}

static void vga_init() {
	// set IOS flag to place CRt regs in 0x3D? range
	uint8_t misc = in8(0x3CC);
	misc |= 0x01;
	out8(0x3C2, misc);
	// get current cursor location
	out8(0x3D4, 0xF);
	vga_cursor |= in8(0x3D5);
	out8(0x3D4, 0xE);
	vga_cursor |= in8(0x3D5) << 8;
}


static void vga_update() {
	while (vga_cursor >= VGA_ROWS * VGA_COLS) {
		// scroll up
		volatile struct vga_grid *grid = vga_frame_buffer.grid;
		for (unsigned int row = 1; row < VGA_ROWS; row++)
			grid->rows[row - 1] = grid->rows[row];
		// clear last row
		volatile struct vga_row *toclear = &grid->rows[VGA_ROWS - 1];
		vga_character_cell empty = {' ', 0x07};
		for (unsigned int col = 0; col < VGA_COLS; col++)
			toclear->cols[col] = empty;
		// fix cursor
		vga_cursor -= VGA_COLS;
	}
	out8(0x3D4, 0xF);
	out8(0x3D5, (vga_cursor >> 0) & 0xFF);
	out8(0x3D4, 0xE);
	out8(0x3D5, (vga_cursor >> 8) & 0xFF);
}

static void vga_putc(char c) {
	if (c == '\n') {
		vga_cursor -= vga_cursor % VGA_COLS;
		vga_cursor += VGA_COLS;
	} else {
		vga_character_cell cell = { c, 0x07 };
		vga_frame_buffer.direct[vga_cursor++] = cell;
	}
	vga_update();
}

static void vga_puts(const char *c) {
	while (*c)
		vga_putc(*c++);
}

static void vga_puthex64(uint64_t val) {
	vga_puts("0x");
	if (val == 0) {
		vga_putc('0');
		return;
	}
	bool on = false;
	for (unsigned int top = 64; top != 0; top -= 4) {
		unsigned int chr = (val >> (top - 4)) & 15;
		on = on || chr;
		if (on) {
			if (chr < 10)
				chr += '0';
			else
				chr += 'A' - 10;
			vga_putc(chr);
		}
	}
}

static void vga_putuint64(uint64_t val) {
	if (val < 10) {
		vga_putc('0' + val);
		return;
	}
	static const uint64_t powers_of_ten[] = {
		10000000000000000000ull, 1000000000000000000ull, 100000000000000000ull, 10000000000000000ull,
		1000000000000000ull, 100000000000000ull, 10000000000000ull, 1000000000000ull,
		100000000000ull, 10000000000ull, 1000000000ull, 100000000ull,
		10000000ull, 1000000ull, 100000ull, 10000ull,
		1000ull, 100ull, 10ull, 1ull
	};
	const uint64_t *pot = powers_of_ten;
	while (*pot > val)
		pot++;
	while (pot < powers_of_ten + 20) {
		char c = '0';
		while (val >= *pot) {
			val -= *pot;
			c++;
		}
		vga_putc(c);
		pot++;
	}
}

__attribute__((noreturn))
static void halt_forever() {
	vga_puts("Finished.\n");
	while (true)
		asm volatile("hlt");
}

static bool check_a20() {
	static const uint32_t patterns[4] = {
		0xDEADBEEF, 
		0xCAFEF00D,
		0xBEDF00D5,
		0xBA5ECA5E,
	};
	for (size_t i = 0; i < sizeof(patterns)/sizeof(patterns[0]); i++) {
		uint32_t *mirror = (uint32_t *)((uint32_t)&patterns[i] + 0x100000);
		if (patterns[i] != *mirror)
			return true;
	}
	return false;
}

static void enable_a20() {
	if (check_a20())
		return;
	{
		bios_register_set rs = { .eax = 0x2401 };
		bios_call_service(0x15, &rs);
	}
	if (check_a20())
		return;
	{
		vga_puts("Couldn't enable A20 line!\n");
		halt_forever();
	}
}

static void maximal_offset(const void *addr, uint16_t *es, uint32_t *reg) {
	uint32_t val = (uint32_t)addr;
	uint32_t seg = val >> 4;
	uint32_t off = val & 0x0F;
	if (seg > 0xFFFF) {
		vga_puts("Pointer to real mode over 1MB!\n");
		halt_forever();
	}
	*es = seg;
	*reg = off;
}

static void read_sector(uint64_t sector_lba, void *dest_buffer) {
	struct {
		uint16_t packet_size;
		uint16_t sector_count;
		uint16_t addr_offset;
		uint16_t addr_segment;
		uint64_t sector_lba;
	} disk_access_packet = {
		.packet_size = 16,
		.sector_count = 1,
		.sector_lba = sector_lba,
	};
	uint16_t es;
	uint32_t reg;
	static unsigned char scratch_sector[512];
	maximal_offset(scratch_sector, &es, &reg);
	disk_access_packet.addr_offset = reg;
	disk_access_packet.addr_segment = es;
	// do bios call
	extern uint8_t bios_drive_index;
	bios_register_set regset = { .eax = 0x4200, .edx = bios_drive_index };
	maximal_offset(&disk_access_packet, &regset.ds, &regset.esi);
	bios_call_service(0x13, &regset);
	// check return code
	uint8_t rc = regset.eax >> 8;
	if (rc) {
		vga_puts("Error ");
		vga_putuint64(rc);
		vga_puts(" while reading sector ");
		vga_putuint64(sector_lba);
		vga_puts(" from hard drive!\n");
		halt_forever();
	}
	// copy from scratch to destination
	__builtin_memcpy(dest_buffer, scratch_sector, 512);
}

static void elf_sanity_check(Elf64_Ehdr *elf_header) {
	unsigned char *ident = elf_header->e_ident;
	if (*(uint32_t *)ident != *(uint32_t *)ELFMAG) {
		vga_puts("Kernel is not an ELF object!\n");
		goto bad_elf;
	}
	if (ident[EI_CLASS] != ELFCLASS64) {
		vga_puts("Kernel is not 64-bit!\n");
		goto bad_elf;
	}
	if (ident[EI_DATA] != ELFDATA2LSB) {
		vga_puts("Kernel is not little-endian!\n");
		goto bad_elf;
	}
	if (ident[EI_VERSION] != EV_CURRENT) {
		vga_puts("Kernel has invalid ELF version!\n");
		goto bad_elf;
	}
	if (elf_header->e_machine != EM_X86_64) {
		vga_puts("Kernel not built for x86_64!\n");
		goto bad_elf;
	}
	return;
bad_elf:
	vga_puts("Invalid ELF object.\n");
	halt_forever();
}

static void *pal_alloc(size_t sz) {
	// round up
	sz +=  0xFFFu;
	sz &= ~0xFFFu;
	// allocate it
	void *ptr = pal_alloc_end;
	pal_alloc_end += sz;
	return ptr;
}

static void map_page(uint64_t virt, uint64_t phys) {
	// PML4
	uint64_t *pml4 = initial_pml4;
	unsigned int pml4i = (virt >> 39) & 0x1FFul;
	uint64_t *pml4e = &pml4[pml4i];
	if (!*pml4e) {
		uint64_t *t = pal_alloc(4096);
		__builtin_memset(t, 0, 4096);
		*pml4e = (uintptr_t)t |
			PAGE_DESC_PRESENT |
			PAGE_DESC_WRITABLE |
			PAGE_DESC_PRIVILEGED;
	}
	// PDPT
	uint64_t *pdpt = (uint64_t *)(uintptr_t)(*pml4e & ~0xFFFul);
	unsigned int pdpti = (virt >> 30) & 0x1FFu;
	uint64_t *pdpte = &pdpt[pdpti];
	if (!*pdpte) {
		uint64_t *t = pal_alloc(4096);
		__builtin_memset(t, 0, 4096);
		*pdpte = (uintptr_t)t |
			PAGE_DESC_PRESENT |
			PAGE_DESC_WRITABLE |
			PAGE_DESC_PRIVILEGED;
	}
	// PD
	uint64_t *pd = (uint64_t *)(uintptr_t)(*pdpte & ~0xFFFul);
	unsigned int pdi = (virt >> 21) & 0x1FFu;
	uint64_t *pde = &pd[pdi];
	if (!*pde) {
		uint64_t *t = pal_alloc(4096);
		__builtin_memset(t, 0, 4096);
		*pde = (uintptr_t)t |
			PAGE_DESC_PRESENT |
			PAGE_DESC_WRITABLE |
			PAGE_DESC_PRIVILEGED;
	}
	// PT
	uint64_t *pt = (uint64_t *)(uintptr_t)(*pde & ~0xFFFul);
	unsigned int pti = (virt >> 12) & 0x1FFu;
	uint64_t *pte = &pt[pti];
	if (!*pte) {
		*pte = (uintptr_t)phys |
			PAGE_DESC_PRESENT |
			PAGE_DESC_WRITABLE |
			PAGE_DESC_PRIVILEGED;
	} else {
		vga_puts("Page at virtual address ");
		vga_puthex64(virt);
		vga_puts(" already mapped to physical address ");
		vga_puthex64(*pte & ~0xFFFul);
		vga_puts("!\n");
	}
}

void ss_entry() {

	// zero out BSS variables
	clear_bss();

	// enable VGA
	vga_init();
	vga_puts("In protected mode.\n");

	// enable A20 line if it's disabled
	enable_a20();

	// get memory map
	e820_entry smap_entry;
	bios_register_set regset = {};
	do {
		regset.eax = 0xe820;
		regset.ecx = 20;
		regset.edx = 0x534d4150;
		maximal_offset(&smap_entry, &regset.es, &regset.edi);
		bios_call_service(0x15, &regset);
		if (regset.eax == 0x534d4150) {
			smap_entry.present = 1;
			e820_entries[e820_entries_count++] = smap_entry;
		} else {
			vga_puts("BIOS does not support e820!\n");
			halt_forever();
		}
	} while (regset.ebx);
	e820_entries[e820_entries_count] = (e820_entry){};

	// find largest usable section under 4GB
	e820_entry *largest_entry = NULL;
	uint32_t largest_efflen = 0;
	for (size_t i = 0; i < e820_entries_count; i++) {
		e820_entry *ent = &e820_entries[i];
		if (ent->type != 0x01)
			continue;
		if (ent->base >> 32)
			continue;
		uint32_t efflen = -(uint32_t)ent->base;
		if (efflen < ent->length)
			efflen = ent->length;
		if (efflen > largest_efflen) {
			largest_entry = ent;
			largest_efflen = efflen;
		}
	}
	if (!largest_entry) {
		vga_puts("Couldn't locate usable memory!\n");
		halt_forever();
	}
	vga_puts("Largest region of memory is ");
	vga_putuint64(largest_entry->length);
	vga_puts(" bytes.\n");

	// set up page table allocator
	uint32_t pal_alloc_base = largest_entry->base;
	pal_alloc_base +=  0xFFFu;
	pal_alloc_base &= ~0xFFFu;
	pal_alloc_start = (unsigned char *)pal_alloc_base;
	pal_alloc_end = (unsigned char *)pal_alloc_base;

	// find first active partition
	partition_entry *kernel_ptentry = NULL;
	for (unsigned int i = 0; i < 4; i++) {
		extern partition_entry partition_table_entries[4];
		if (partition_table_entries[i].part_status & 0x80) {
			kernel_ptentry = &partition_table_entries[i];
			break;
		}
	}
	vga_puts("Kernel partition is ");
	vga_putuint64(kernel_ptentry->sector_count);
	vga_puts(" sectors large.\n");

	// copy kernel into memory
	uint32_t kernel_sector_start = kernel_ptentry->first_sector;
	uint32_t kernel_sector_count = kernel_ptentry->sector_count;
	uint32_t kernel_size = kernel_sector_count * 512;
	unsigned char *kernel_start = pal_alloc(kernel_size);
	unsigned char *kernel_end = kernel_start;
	for (uint32_t s = 0; s < kernel_sector_count; s++) {
		read_sector(kernel_sector_start + s, kernel_end);
		kernel_end += 512;
	}
	vga_putuint64(kernel_size);
	vga_puts(" bytes of kernel copied into memory.\n");

	// perform sanity check on kernel
	Elf64_Ehdr *elf_header = (Elf64_Ehdr *)kernel_start;
	elf_sanity_check(elf_header);

	// create page table
	initial_pml4 = pal_alloc(4096);
	__builtin_memset(initial_pml4, 0, 4096);

	// map kernel into memory
	unsigned char *phdr_ptr = kernel_start + elf_header->e_phoff;
	unsigned int phdr_count = elf_header->e_phnum;
	unsigned int phdr_stride = elf_header->e_phentsize;
	for (unsigned int i = 0; i < phdr_count; i++, phdr_ptr += phdr_stride) {
		Elf64_Phdr *phdr_ent = (Elf64_Phdr *)phdr_ptr;
		if (phdr_ent->p_type != PT_LOAD) {
			continue;
		}
		if (phdr_ent->p_align != 0x1000) {
			vga_puts("Only 4096-byte alignment supported but ");
			vga_putuint64(phdr_ent->p_align);
			vga_puts("-byte alignment requested!\n");
			halt_forever();
		}
		if (phdr_ent->p_filesz > phdr_ent->p_memsz) {
			vga_puts("PH file size is larger than memory size!\n");
			halt_forever();
		}
		vga_puts("Mapping ");
		vga_putuint64(phdr_ent->p_filesz);
		vga_putc('/');
		vga_putuint64(phdr_ent->p_memsz);
		vga_puts(" bytes at ");
		vga_puthex64(phdr_ent->p_vaddr);
		vga_puts(".\n");
		uint64_t phys_addr = (uintptr_t)(kernel_start + phdr_ent->p_offset);
		uint64_t virt_addr = phdr_ent->p_vaddr;
		uint32_t bytes_mapped = 0;
		while (bytes_mapped < phdr_ent->p_filesz) {
			map_page(virt_addr, phys_addr);
			phys_addr += 4096;
			virt_addr += 4096;
			bytes_mapped += 4096;
		}
		size_t zero_size = (phdr_ent->p_memsz - bytes_mapped + 0xFFFu) & ~0xFFFu;
		uint64_t zero_addr = (uintptr_t)pal_alloc(zero_size);
		while (bytes_mapped < phdr_ent->p_memsz) {
			map_page(virt_addr, zero_addr);
			zero_addr += 4096;
			virt_addr += 4096;
			bytes_mapped += 4096;
		}
	}

	// identity map ourselves starting from 16KB
	for (uint32_t addr = 0x4000; addr < 0x200000; addr += 0x1000)
		map_page(addr, addr);

	// enable 64-bit paging
	asm volatile("mov %0, %%cr3" :: "r"(initial_pml4));
	asm volatile("wrmsr" :: "c"(IA32_EFER), "A"((uint64_t)IA32_EFER_LME));
	asm volatile(
			"mov %%cr4, %%eax\n\t"
			"or %0, %%eax\n\t"
			"mov %%eax, %%cr4\n\t"
			:: "n"(CR4_PAE) : "eax");
	asm volatile(
			"mov %%cr0, %%eax\n\t"
			"or %0, %%eax\n\t"
			"mov %%eax, %%cr0\n\t"
			:: "n"(CR0_PG) : "eax");

	// CAUTION: until a page fault handler is registered,
	// any page faults from here on will crash the system.

	// jump to kernel!
	extern void enter_kernel(uint64_t addr);
	enter_kernel(elf_header->e_entry);
}
#endif

#include <stdint.h>

#define __init __attribute__((section(".text.init")))
#define __initdata __attribute__((section(".data.init")))

extern uint8_t mbr_start[];
extern unsigned char ss_start, ss_end;

extern uint8_t read_single_sector(uint32_t lba, void *buf);
extern void write_character(char c);

static const char __initdata msg_info_halting[] = "Halted.\r\n";
static const char __initdata msg_err_disk_read[] = "Disk read failed!\r\n";

void mbr_entry_32c();

static void __init write_raw(const char *s) {
	char c;
	while ((c = *s++))
		write_character(c);
}

static void __init halt_forever() {
	write_raw(msg_info_halting);
	__asm__ __volatile__("hlt");
	__builtin_unreachable();
}

void __init mbr_entry_32b() {
	unsigned int ss_bytes = &ss_end - &ss_start;
	unsigned int ss_sectors = (ss_bytes + 0x1FF) >> 9;
	for (unsigned int i = 1; i <= ss_sectors; i++) {
		if (read_single_sector(i, mbr_start + (i << 9)) != 0) {
			write_raw(msg_err_disk_read);
			halt_forever();
		}
	}
	mbr_entry_32c();
}

void mbr_entry_32c() {
	halt_forever();
}
