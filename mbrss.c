#define asm __asm__

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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
	uint32_t num_sectors;
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
unsigned char scratch_sector[512];

extern partition_entry partition_table_entries[4];

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
	char buf[19];
	char *ptr = &buf[18];
	*ptr = '\0';
	do {
		uint8_t seg = val & 0xF;
		if (seg < 10)
			seg += '0';
		else
			seg += 'A' - 10;
		*--ptr = seg;
		val >>= 4;
	} while (val != 0);
	*--ptr = 'x';
	*--ptr = '0';
	vga_puts(ptr);
}

static void vga_putuint64(uint64_t val) {
	char buf[21];
	char *ptr = &buf[20];
	*ptr = '\0';
	do {
		uint64_t hi = val / 10u;
		uint8_t lo = val % 10u;
		*--ptr = lo + '0';
		val = hi;
	} while (val != 0);
	vga_puts(ptr);
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

	// find largest usable section
	e820_entry *largest_entry = NULL;
	for (size_t i = 0; i < e820_entries_count; i++) {
		e820_entry *ent = &e820_entries[i];
		if (ent->type != 0x01)
			continue;
		if (!largest_entry || ent->length > largest_entry->length)
			largest_entry = ent;
	}
	if (!largest_entry) {
		vga_puts("Couldn't locate usable memory!\n");
		halt_forever();
	}
	vga_puts("Largest region of memory is ");
	vga_puthex64(largest_entry->length);
	vga_puts(" bytes.\n");

	vga_puts("We shall now sleep forever.\n");
	halt_forever();

}
