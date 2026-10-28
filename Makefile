CC = gcc
CFLAGS = -ffreestanding -m64 -mno-red-zone -mgeneral-regs-only -O2 -Wall -Wextra \
         -Iinclude -fno-stack-protector -fno-pic -fno-pie -mcmodel=large
LDFLAGS = -nostdlib -static -T linker.ld -no-pie

SRCS = \
	src/boot/kernel.c \
	src/core/string.c \
	src/core/console.c \
	src/core/serial.c \
	src/core/mem.c \
	src/drivers/keyboard.c \
	src/drivers/mouse.c \
	src/drivers/rtc.c \
	src/drivers/ata.c \
	src/gui/gfx.c \
	src/gui/ui.c \
	src/gui/menu.c \
	src/gui/wm.c \
	src/gui/desktop.c \
	src/fs/fs.c \
	src/fs/storage.c \
	src/apps/app_term.c \
	src/apps/app_files.c \
	src/apps/app_edit.c \
	src/apps/app_calc.c \
	src/apps/app_set.c \
	src/apps/app_trash.c \
	src/apps/app_inst.c \
	src/apps/app_about.c

OBJS = $(SRCS:src/%.c=build/%.o)
HDRS = $(wildcard include/*.h)

all: build/kernel.elf

build/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	@echo "Built $@"

# Object files and the kernel only. The disk image is intentionally NOT
# removed here: deleting build/disk.vdi is what made VirtualBox fail with
# "file not found" after every clean. Use distclean for that.
clean:
	rm -f build/kernel.elf iso/boot/kernel.elf
	@find build -name '*.o' -delete 2>/dev/null || true

distclean:
	rm -rf build

.PHONY: all clean distclean iso disk run

# The ISO embeds iso/boot/kernel.elf, so it must be refreshed before packing.
iso: all
	cp build/kernel.elf iso/boot/kernel.elf
	rm -f build/denemeos.iso
	genisoimage -R -r -J -o build/denemeos.iso \
		-b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 \
		-boot-info-table -e boot/limine/limine-uefi-cd.bin -no-emul-boot iso/
	$(CURDIR)/tools/limine bios-install --force build/denemeos.iso
	cp build/kernel.elf build/.iso-kernel

# Disk layout (96 MiB):
#   LBA 0              MBR + Limine BIOS stage 1
#   LBA 2048..131071   FAT32 partition 1 (kernel + Limine files)
#   LBA 131072..135167 DenemeOS snapshot area (2 MiB, outside FAT32)
disk: all
	LC_ALL=C rm -f build/disk.img build/disk.vdi.new
	dd if=/dev/zero of=build/disk.img bs=1M count=96 status=none
	printf 'o\nn\np\n1\n2048\n131071\n\nt\nc\nw\n' | LC_ALL=C fdisk build/disk.img >/dev/null
	LC_ALL=C mformat -i build/disk.img@@2048s -T 129024 -F ::
	LC_ALL=C mmd -i build/disk.img@@2048s ::/boot
	LC_ALL=C mmd -i build/disk.img@@2048s ::/boot/limine
	LC_ALL=C mmd -i build/disk.img@@2048s ::/EFI
	LC_ALL=C mmd -i build/disk.img@@2048s ::/EFI/BOOT
	LC_ALL=C mcopy -i build/disk.img@@2048s build/kernel.elf ::/boot/kernel.elf
	LC_ALL=C mcopy -i build/disk.img@@2048s iso/boot/limine/limine-bios.sys ::/boot/limine/limine-bios.sys
	LC_ALL=C mcopy -i build/disk.img@@2048s iso/boot/limine/limine.conf ::/boot/limine/limine.conf
	LC_ALL=C mcopy -i build/disk.img@@2048s iso/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	$(CURDIR)/tools/limine bios-install build/disk.img 1
	VBoxManage convertfromraw build/disk.img build/disk.vdi.new --format VDI
	mv -f build/disk.vdi.new build/disk.vdi
	cp build/kernel.elf build/.disk-kernel

run: disk
	./run-vbox-disk.sh
