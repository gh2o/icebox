#define SEG_DESC_BASE_PACK_HI(x) \
	(((x) & 0xFF000000) | (((x) >> 16) & 0xFF))
#define SEG_DESC_BASE_PACK_LO(x) \
	(((x) << 16) & 0xFFFF0000)
#define SEG_DESC_LIMIT_PACK_HI(x) \
	((x) & 0xF0000)
#define SEG_DESC_LIMIT_PACK_LO(x) \
	((x) & 0xFFFF)

#define SEG_DESC_G       (1 << 23)
#define SEG_DESC_32      (1 << 22)
#define SEG_DESC_64      (1 << 21)
#define SEG_DESC_AVL     (1 << 20)
#define SEG_DESC_P       (1 << 15)
#define SEG_DESC_DPL(x)  ((x & 3) << 13)
#define SEG_DESC_S       (1 << 12)
#define SEG_DESC_TYPE(x) ((x & 15) << 8)

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
