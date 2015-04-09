CFLAGS_32 := -m32 -nostdlib -ggdb -Os -Wall -std=c99 -ffunction-sections -fdata-sections \
	-fomit-frame-pointer -mpreferred-stack-boundary=2
LDFLAGS_32 := -Wl,-q,--build-id=none -lgcc

RUSTFLAGS := -C debuginfo -C opt-level=2 -C no-stack-check
CFLAGS := -m64 -nostdlib -ggdb
LDFLAGS := -Wl,-q,--build-id=none -lgcc -Wl,-n

KERNEL_OFFSET := 1048576

MBRSS_SRCS := mbrss.S mbrss.c
KERNEL_SRCS := kernel.S kernel.rs

all: build/disk.img

run: build/disk.img
	qemu-system-x86_64 build/disk.img

.PHONY: all run
.DELETE_ON_ERROR:

build/disk.qcow: build/disk.img
	qemu-img convert -O qcow $< $@

build/disk.img: build/mbrss.bin build/kernel.elf
	@wc -c build/mbrss.bin | awk '{exit $$1 >= $(KERNEL_OFFSET)}' || (echo 'MBR+SS is too big!' && false)
	cat build/mbrss.bin > $@
	truncate -s $(KERNEL_OFFSET) $@
	cat build/kernel.elf >> $@
	truncate -s %$(KERNEL_OFFSET) $@
	wc -c build/kernel.elf | awk ' \
		function c(x){match(sprintf("%08x",x),"(..)(..)(..)(..)",a);return a[4]a[3]a[2]a[1]} \
		{print c(0x80)c(0x83)c(rshift($(KERNEL_OFFSET),9))c(rshift($$1+511,9))} \
		' | xxd -r -p | dd bs=1 seek=446 conv=notrunc status=none of=$@

MBRSS_OBJS := $(patsubst %,build/mbrss/%.o,$(MBRSS_SRCS))
build/mbrss.bin: build/mbrss.elf
	objcopy -O binary $< $@
build/mbrss.elf: $(MBRSS_OBJS) mbrss.x
	gcc $(MBRSS_OBJS) -o $@ -Wl,-T,mbrss.x $(CFLAGS_32) $(LDFLAGS_32)
build/mbrss/%.o: %
	@mkdir -p build/mbrss
	gcc $^ -c -o $@ $(CFLAGS_32)

KERNEL_OBJS := $(patsubst %,build/kernel/%.o,$(KERNEL_SRCS))
build/kernel.elf: $(KERNEL_OBJS) kernel.x
	gcc $(KERNEL_OBJS) -o $@ -Wl,-T,kernel.x $(CFLAGS) $(LDFLAGS)
build/kernel/%.rs.o: %.rs
	@mkdir -p build/kernel
	rustc --emit=obj --crate-type=lib $^ -o $@ $(RUSTFLAGS)
build/kernel/%.S.o: %.S
	@mkdir -p build/kernel
	gcc $^ -c -o $@ $(CFLAGS)
