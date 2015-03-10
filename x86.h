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
#define SEG_DESC_L       (1 << 21)
#define SEG_DESC_AVL     (1 << 20)
#define SEG_DESC_P       (1 << 15)
#define SEG_DESC_DPL(x)  ((x & 3) << 13)
#define SEG_DESC_S       (1 << 12)
#define SEG_DESC_TYPE(x) ((x & 15) << 8)

#define CR0_PE (1 << 0)
