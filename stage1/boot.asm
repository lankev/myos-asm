; =============================================================
; MyOS - Stage1 MBR
; Active A20, charge MineGRUB (stage2) a 0x8000 via LBA
; Fonctionne sur floppy, disque dur ET cle USB
; =============================================================
[BITS 16]
[ORG 0x7C00]

STAGE2_SEG   equ 0x0800         ; 0x0800 * 16 = 0x8000
STAGE2_SECTS equ 16             ; secteurs de MineGRUB

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

    ; Charge MineGRUB via LBA etendu (Int 13h/AH=42h)
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc  .disk_err

    jmp 0x0000:0x8000

.disk_err:
    mov ah, 0x0E
    mov al, 'E'
    xor bh, bh
    int 0x10
    jmp $

; =============================================================
; Disk Address Packet (DAP) pour Int 13h/AH=42h
; =============================================================
dap:
    db 0x10             ; taille du paquet (16 octets)
    db 0                ; reserve
    dw STAGE2_SECTS     ; nombre de secteurs a lire
    dw 0x0000           ; offset destination
    dw STAGE2_SEG       ; segment destination (0x0800 -> 0x8000)
    dq 1                ; LBA de depart (secteur 1, apres MBR)

boot_drive db 0

times 510-($-$$) db 0
dw 0xAA55
