; =============================================================
; MyOS - Stage1 MBR
; Active A20, charge MineGRUB (stage2) a 0x8000, saute dedans
; =============================================================
[BITS 16]
[ORG 0x7C00]

STAGE2_SEG   equ 0x0800         ; 0x0800 * 16 = 0x8000
STAGE2_SECTS equ 16             ; secteurs de MineGRUB
STAGE2_START equ 2              ; secteur CHS de depart (apres MBR)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Active A20 (fast gate)
    in  al, 0x92
    or  al, 0x02
    and al, 0xFE
    out 0x92, al

    ; Charge MineGRUB a STAGE2_SEG:0x0000 = 0x8000
    mov ax, STAGE2_SEG
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, STAGE2_SECTS
    mov ch, 0
    mov cl, STAGE2_START
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc  .disk_err

    ; Saute dans MineGRUB (DL = boot drive toujours dans dl)
    jmp 0x0000:0x8000

.disk_err:
    mov ah, 0x0E
    mov al, 'E'
    xor bh, bh
    int 0x10
    jmp $

; =============================================================
; Data
; =============================================================
boot_drive db 0

; Padding MBR
times 510-($-$$) db 0
dw 0xAA55
