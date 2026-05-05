AS   = nasm
QEMU = qemu-system-i386
BUILD = build

# Compilateur C freestanding 32-bit
CC   = gcc
CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles \
         -fno-pic -fno-pie \
         -O2 -Wall -Wextra -Wno-array-bounds \
         -Ilibc/include -Isfcml/include

AR   = ar

# Layout disque (en secteurs de 512 octets) :
#   Secteur 0       : Stage1 MBR       (1 secteur)
#   Secteurs 1-16   : MineGRUB stage2  (16 secteurs = 8 Ko)
#   Secteurs 17-76  : Kernel           (60 secteurs)

LIBC_SRC = libc/src/string.c libc/src/stdlib.c libc/src/stdio.c
SFCML_SRC = sfcml/src/window.c sfcml/src/graphics.c sfcml/src/events.c

LIBC_OBJ  = $(patsubst %.c, $(BUILD)/%.o, $(LIBC_SRC))
SFCML_OBJ = $(patsubst %.c, $(BUILD)/%.o, $(SFCML_SRC))

.PHONY: all libs clean run debug

all: $(BUILD)/myos.img
	@echo ""
	@echo "  MyOS + MineGRUB + libc + SFCML compiles! Lance: make run"

libs: $(BUILD)/libk.a $(BUILD)/libsfcml.a
	@echo "[LIBS] libk.a et libsfcml.a prets"

# Regles compilation C
$(BUILD)/libc/src/%.o: libc/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sfcml/src/%.o: sfcml/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libk.a: $(LIBC_OBJ)
	@mkdir -p $(BUILD)
	$(AR) rcs $@ $^
	@echo "[AR] $@"

$(BUILD)/libsfcml.a: $(SFCML_OBJ)
	@mkdir -p $(BUILD)
	$(AR) rcs $@ $^
	@echo "[AR] $@"

$(BUILD)/myos.img: $(BUILD)/boot.bin $(BUILD)/minegrub.bin $(BUILD)/kernel.bin
	@mkdir -p $(BUILD)
	dd if=/dev/zero           bs=512 count=2880 of=$@            2>/dev/null
	dd if=$(BUILD)/boot.bin   bs=512 count=1    of=$@ seek=0  conv=notrunc 2>/dev/null
	dd if=$(BUILD)/minegrub.bin bs=512 count=16 of=$@ seek=1  conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin bs=512 count=120  of=$@ seek=17 conv=notrunc 2>/dev/null
	@echo "[IMG] $(BUILD)/myos.img pret"
	@ls -lh $(BUILD)/

$(BUILD)/boot.bin: stage1/boot.asm
	@mkdir -p $(BUILD)
	$(AS) -f bin -o $@ $<
	@echo "[BOOT] $@ ($(shell wc -c < $@) octets)"

$(BUILD)/minegrub.bin: stage2/minegrub.asm
	@mkdir -p $(BUILD)
	$(AS) -f bin -o $@ $<
	@echo "[GRUB] $@ ($(shell wc -c < $@) octets)"

# Kernel C (crt0 + kmain + libs)
$(BUILD)/kernel/crt0.o: kernel/crt0.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 -o $@ $<

$(BUILD)/kernel/kmain.o: kernel/kmain.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel/crt0.o $(BUILD)/kernel/kmain.o \
                     $(BUILD)/libsfcml.a $(BUILD)/libk.a
	ld -m elf_i386 -T kernel/kernel.ld -o $@ \
		$(BUILD)/kernel/crt0.o $(BUILD)/kernel/kmain.o \
		-L$(BUILD) -lsfcml -lk
	@echo "[LD] $@"

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	objcopy -O binary $< $@
	@echo "[KERN] $@"

QFLAGS = -drive file=$(BUILD)/myos.img,format=raw,if=floppy \
         -boot a -no-reboot -no-shutdown \
         -vga std -m 128M

run: all
	$(QEMU) $(QFLAGS)

debug: all
	$(QEMU) $(QFLAGS) -s -S

clean:
	rm -rf $(BUILD)
