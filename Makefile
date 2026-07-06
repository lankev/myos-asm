AS   = nasm
QEMU = qemu-system-i386
BUILD = build

# Compilateur C freestanding 32-bit
CC   = gcc
CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles \
         -fno-pic -fno-pie \
         -O2 -Wall -Wextra -Wno-array-bounds \
         -fno-asynchronous-unwind-tables \
         -Ilibc/include -Isfcml/include -Inet

AR   = ar

# Layout disque (en secteurs de 512 octets) :
#   Secteur 0       : Stage1 MBR       (1 secteur)
#   Secteurs 1-16   : MineGRUB stage2  (16 secteurs = 8 Ko)
#   Secteurs 17-76  : Kernel           (60 secteurs)

LIBC_SRC = libc/src/string.c libc/src/stdlib.c libc/src/stdio.c
SFCML_SRC = sfcml/src/window.c sfcml/src/graphics.c sfcml/src/events.c
NET_SRC  = net/net.c net/tls.c

LIBC_OBJ  = $(patsubst %.c, $(BUILD)/%.o, $(LIBC_SRC))
SFCML_OBJ = $(patsubst %.c, $(BUILD)/%.o, $(SFCML_SRC))
NET_OBJ   = $(patsubst %.c, $(BUILD)/%.o, $(NET_SRC))

.PHONY: all libs clean run run-asm debug debug-asm

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
	dd if=$(BUILD)/kernel.bin bs=512 count=128  of=$@ seek=17  conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin bs=512 skip=128 count=128 of=$@ seek=145 conv=notrunc 2>/dev/null || true
	dd if=$(BUILD)/kernel.bin bs=512 skip=256 count=128 of=$@ seek=273 conv=notrunc 2>/dev/null || true
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

$(BUILD)/net/%.o: net/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel/crt0.o $(BUILD)/kernel/kmain.o \
                     $(NET_OBJ) \
                     $(BUILD)/libsfcml.a $(BUILD)/libk.a
	ld -m elf_i386 -T kernel/kernel.ld -o $@ \
		$(BUILD)/kernel/crt0.o $(BUILD)/kernel/kmain.o \
		$(NET_OBJ) \
		-L$(BUILD) -lsfcml -lk
	@echo "[LD] $@"

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	objcopy -O binary $< $@
	@echo "[KERN] $@"

# Audio : le PC speaker (PIT canal 2) route vers l'hote pour un son reel.
# SDL marche sur Linux/WSLg et Windows ; ajuste AUDIODRV si besoin (dsound, pa, coreaudio).
AUDIODRV ?= sdl
AUDIOFLAGS = -audiodev $(AUDIODRV),id=snd0 -machine pcspk-audiodev=snd0

QFLAGS = -drive file=$(BUILD)/myos.img,format=raw,if=ide \
         -boot c -no-reboot -no-shutdown \
         -vga std -m 128M \
         -display gtk,zoom-to-fit=on \
         $(AUDIOFLAGS) \
         -netdev user,id=net0 -device rtl8139,netdev=net0

QFLAGS_ASM = -drive file=$(BUILD)/myos_asm.img,format=raw,if=ide \
             -boot c -no-reboot -no-shutdown \
             -vga std -m 128M \
             -display gtk,zoom-to-fit=on \
             -netdev user,id=net0 -device rtl8139,netdev=net0

run: all
	$(QEMU) $(QFLAGS)

debug: all
	$(QEMU) $(QFLAGS) -s -S

# ---------------------------------------------------------------
# Kernel ASM pur (kernel/kernel.asm standalone)
# ---------------------------------------------------------------
$(BUILD)/kernel_asm.bin: kernel/kernel.asm
	@mkdir -p $(BUILD)
	$(AS) -f bin -o $@ $<
	@echo "[ASM] $@ ($(shell wc -c < $@) octets)"

$(BUILD)/myos_asm.img: $(BUILD)/boot.bin $(BUILD)/minegrub.bin $(BUILD)/kernel_asm.bin
	@mkdir -p $(BUILD)
	dd if=/dev/zero              bs=512 count=2880 of=$@            2>/dev/null
	dd if=$(BUILD)/boot.bin      bs=512 count=1    of=$@ seek=0  conv=notrunc 2>/dev/null
	dd if=$(BUILD)/minegrub.bin  bs=512 count=16   of=$@ seek=1  conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel_asm.bin bs=512 count=256  of=$@ seek=17 conv=notrunc 2>/dev/null || true
	@echo "[IMG] $(BUILD)/myos_asm.img pret"
	@ls -lh $(BUILD)/myos_asm.img

run-asm: $(BUILD)/myos_asm.img
	@echo "  Lancement du kernel ASM pur dans QEMU..."
	$(QEMU) $(QFLAGS_ASM)

debug-asm: $(BUILD)/myos_asm.img
	@echo "  Debug kernel ASM: connecte gdb sur localhost:1234"
	$(QEMU) $(QFLAGS_ASM) -s -S

clean:
	rm -rf $(BUILD)

# =============================================================
# Ecriture sur cle USB (Linux / WSL)
# Usage: make usb DEV=/dev/sdX   (ex: make usb DEV=/dev/sdb)
# ATTENTION: efface toutes les donnees de la cle !
# Trouver la cle : lsblk  ou  dmesg | tail -20
# =============================================================
DEV ?= /dev/sdb
usb: $(BUILD)/myos.img
	@echo ""
	@echo "  ================================================"
	@echo "  ATTENTION: Ecriture de myos.img sur $(DEV)"
	@echo "  Toutes les donnees sur $(DEV) seront perdues!"
	@echo "  Ctrl+C pour annuler, Entree pour continuer..."
	@echo "  ================================================"
	@read dummy
	dd if=$(BUILD)/myos.img of=$(DEV) bs=512 conv=notrunc,fsync
	sync
	@echo "[USB] Image ecrite sur $(DEV) - OK"
