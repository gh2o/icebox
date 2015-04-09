#define SEG_DESC_PACK(base, limit) ( \
	((base << 32) & (0xFF << 56)) | \
	((limit << 32) & (0x0F << 48)) | \
	((base & 0xFFFFFF) << 16) | \
	(limit & 0xFFFF))

#define SEG_DESC_G       (1 << 55)
#define SEG_DESC_32      (1 << 54)
#define SEG_DESC_64      (1 << 53)
#define SEG_DESC_AVL     (1 << 52)
#define SEG_DESC_P       (1 << 47)
#define SEG_DESC_DPL(x)  ((x & 3) << 45)
#define SEG_DESC_S       (1 << 44)
#define SEG_DESC_TYPE(x) ((x & 15) << 40)

#define PAGE_DESC_PRESENT    (1 << 0)
#define PAGE_DESC_WRITABLE   (1 << 1)
#define PAGE_DESC_PRIVILEGED (1 << 2)
#define PAGE_DESC_WRITETHRU  (1 << 3)
#define PAGE_DESC_UNCACHED   (1 << 4)
#define PAGE_DESC_ACCESSED   (1 << 5)
#define PAGE_DESC_DIRTY      (1 << 6)
#define PAGE_DESC_ISPAGE     (1 << 7) // rather than directory
#define PAGE_DESC_GLOBAL     (1 << 8)

#define CR0_PE  (1 << 0)
#define CR0_PG  (1 << 31)
#define CR4_PAE (1 << 5)

#define IA32_EFER     0xC0000080
#define IA32_EFER_LME (1 << 8)
#define IA32_EFER_LMA (1 << 10)
