CFLAGS_32 := -m32 -nostdlib -ggdb -Os -Wall -std=c99 -ffunction-sections -fdata-sections
LDFLAGS_32 := -Wl,-q -lgcc

all: build/disk.img

run: build/disk.img
	qemu-system-x86_64 build/disk.img

.PHONY: all run
.DELETE_ON_ERROR:

$(shell mkdir -p build)

build/disk.qcow: build/disk.img
	qemu-img convert -O qcow $< $@

build/disk.img: build/mbrss.bin
	cat $< > $@
	truncate -s 1048576 $@

build/mbrss.bin: build/mbrss.elf
	objcopy -O binary $< $@

build/mbrss.elf: build/mbrss.S.o build/mbrss.c.o mbrss.x
	gcc build/mbrss.{S,c}.o -o $@ -Wl,-T,mbrss.x $(CFLAGS_32) $(LDFLAGS_32)

build/mbrss.%.o: mbrss.%
	gcc $^ -c -o $@ $(CFLAGS_32)

#kernel.elf: kernel.o kernel.x
#	ld -T kernel.x kernel.o -o $@ -n -z noexecstack
#
#%.rs.o: %.rs
#	rustc --emit=obj --crate-type=lib -C debuginfo -C opt-level=2 -C no-stack-check $^ -o $@
