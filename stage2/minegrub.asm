; =============================================================
; MineGRUB - Stage2 Bootloader (theme Minecraft)
; Charge par Stage1 a 0x8000 (16-bit real mode)
; Affiche le menu, puis charge le kernel et passe en 32-bit
; =============================================================
[BITS 16]
[ORG 0x8000]

KERNEL_SEG   equ 0x1000       ; adresse lineaire 0x10000
KERNEL_SECTS equ 128          ; 128*512=65536 => chunk 1
KERNEL_CHS_S equ 18           ; CHS sector = LBA 17 + 1 = 18
VGA          equ 0xB800
TIMEOUT      equ 5

; Zone memoire basse pour communiquer avec le kernel
VESA_BUF     equ 0x2000       ; buffer temporaire 256 octets pour int 0x10
BOOT_INFO    equ 0x0500       ; bloc info: [fb_addr:4][w:2][h:2][bpp:1][flag:1]
BOOT_INFO_DRIVE  equ 0x050E   ; byte: boot drive
BOOT_INFO_MEMLO  equ 0x0510   ; word: low mem KB (int 12h)
BOOT_INFO_MEMHI  equ 0x0512   ; word: high mem KB (int 15h/88h)

; =============================================================
; Entry point
; =============================================================
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl
    mov byte [sel], 0
    mov byte [countdown], TIMEOUT

    call cls
    call draw_header
    call draw_creeper
    call draw_menu_box
    call draw_footer
    call menu_loop

    ; never reached
    jmp $

; =============================================================
; Variables
; =============================================================
boot_drive  db 0
sel         db 0           ; 0=MyOS, 1=Reboot
countdown   db TIMEOUT
tick_prev   dw 0

; =============================================================
; Strings
; =============================================================
str_title   db '   M I N E G R U B   -   Boot Manager   ', 0
str_version db '   Powered by MyOS Assembly | v1.0      ', 0
str_myos    db ' > MyOS 1.0  [Lance mon OS perso]       ', 0
str_reboot  db '   Reboot    [Redemarrer la machine]     ', 0
str_keys    db '  Fleches: naviguer   Entree: lancer   E: erreur disk', 0
str_auto    db '  Demarrage automatique dans:  _ secondes  ', 0
str_loading db 'Chargement du kernel...', 0
str_diskerr db 'ERREUR DISQUE ! Appuyez sur une touche.', 0

; (donnees du creeper definies dans draw_creeper)

; Ligne de separation (80 chars)
str_sep  db '--------------------------------------------------------------------------------', 0

; Chiffres pour le countdown
digits db '0123456789'

; =============================================================
; cls: efface l'ecran (fond noir, texte gris)
; =============================================================
cls:
    push es
    push di
    mov ax, VGA
    mov es, ax
    xor di, di
    mov cx, 80*25
    mov al, ' '
    mov ah, 0x07
    rep stosw
    pop di
    pop es
    ret

; =============================================================
; vga_putc: ecrit char AL avec attr AH a (row=BH, col=BL)
; =============================================================
vga_putc:
    push es
    push di
    push ax
    mov dx, VGA
    mov es, dx
    movzx di, bh
    imul di, di, 80
    movzx dx, bl
    add di, dx
    shl di, 1
    pop ax
    stosw
    pop di
    pop es
    ret

; =============================================================
; vga_str: ecrit chaine SI avec attr AH a (row=BH, col=BL)
; =============================================================
vga_str:
    push bx
    push si
    push ax
.loop:
    lodsb
    test al, al
    jz .done
    call vga_putc
    inc bl
    jmp .loop
.done:
    pop ax
    pop si
    pop bx
    ret

; =============================================================
; fill_row: remplit ligne BH avec char AL attr AH sur 80 cols
; =============================================================
fill_row:
    push es
    push di
    push cx
    mov dx, VGA
    mov es, dx
    movzx di, bh
    imul di, di, 80
    shl di, 1
    mov cx, 80
.loop:
    stosw
    loop .loop
    pop cx
    pop di
    pop es
    ret

; =============================================================
; draw_header: bandeau vert Minecraft en haut
; =============================================================
draw_header:
    ; Ligne 0: fond vert uni
    mov bh, 0
    mov al, ' '
    mov ah, 0x20       ; bg=vert(2), fg=noir
    call fill_row

    ; Ligne 1: titre en jaune sur vert
    mov bh, 1
    mov al, ' '
    mov ah, 0x20
    call fill_row
    mov bh, 1
    mov bl, 0
    mov si, str_title
    mov ah, 0x2E       ; bg=vert, fg=jaune
    call vga_str

    ; Ligne 2: fond vert
    mov bh, 2
    mov al, ' '
    mov ah, 0x20
    call fill_row
    ret

; =============================================================
; draw_creeper: pixel art Minecraft (8px x 8rows, col 32, row 4)
;
; Chaque ligne = 1 octet masque (MSB = pixel gauche)
; 1 = bloc vert (0xDB, attr 0x0A)   0 = trou noir (0x20, attr 0x00)
; Chaque pixel = 2 chars pour compenser le ratio hauteur/largeur
;
; Vrai creeper Minecraft:
;   0xFF  ████████████████
;   0xFF  ████████████████
;   0x99  ██    ████    ██  <- yeux (bits 6,5 et 2,1 = trous)
;   0x99  ██    ████    ██
;   0xE7  ██████    ██████  <- nez  (bits 4,3 = trous)
;   0xC3  ████        ████  <- bouche haut (bits 5,4,3,2 = trous)
;   0xDB  ████  ████  ████  <- bouche bas  (bits 5 et 2 = trous)
;   0xFF  ████████████████
; =============================================================
draw_creeper:
    mov si, creeper_mask
    mov bh, 4
    mov cx, 8
.row:
    push cx
    push bx
    lodsb                  ; al = masque de la ligne courante
    mov dl, al
    mov bl, 32             ; colonne de depart
    mov cx, 8              ; 8 pixels par ligne
.pix:
    test dl, 0x80          ; bit MSB = pixel courant
    jz .hole
    push dx
    mov al, 0xDB           ; bloc plein CP437
    mov ah, 0x0A           ; bright green on black
    call vga_putc
    inc bl
    call vga_putc
    inc bl
    pop dx
    jmp .nxt
.hole:
    push dx
    mov al, ' '
    mov ah, 0x00           ; black on black
    call vga_putc
    inc bl
    call vga_putc
    inc bl
    pop dx
.nxt:
    shl dl, 1
    loop .pix
    pop bx
    inc bh
    pop cx
    loop .row

    ; Labels a droite du creeper (col 52, rows 5-7)
    mov bh, 5
    mov bl, 52
    mov si, lbl_minegrub
    mov ah, 0x0E           ; jaune
    call vga_str
    mov bh, 6
    mov bl, 52
    mov si, lbl_bootmgr
    mov ah, 0x0A           ; vert
    call vga_str
    mov bh, 7
    mov bl, 52
    mov si, lbl_edition
    mov ah, 0x08           ; gris fonce
    call vga_str
    ret

lbl_minegrub  db 'M I N E G R U B', 0
lbl_bootmgr   db 'Boot Manager v1.0', 0
lbl_edition   db 'MyOS Assembly Edition', 0

; Pixel art du vrai Creeper Minecraft (bitmask 8 bits/ligne)
creeper_mask:
    db 0xFF   ; ████████████████
    db 0xFF   ; ████████████████
    db 0x99   ; ██    ████    ██  yeux
    db 0x99   ; ██    ████    ██  yeux
    db 0xE7   ; ██████    ██████  nez
    db 0xC3   ; ████        ████  bouche haut
    db 0xDB   ; ████  ████  ████  bouche bas
    db 0xFF   ; ████████████████

; =============================================================
; draw_menu_box: affiche la boite de menu (rows 14-18)
; =============================================================
draw_menu_box:
    ; Separateur haut
    mov bh, 14
    mov bl, 0
    mov si, str_sep
    mov ah, 0x02       ; vert sombre
    call vga_str

    ; Les 2 entrees
    call redraw_entries

    ; Separateur bas
    mov bh, 17
    mov bl, 0
    mov si, str_sep
    mov ah, 0x02
    call vga_str
    ret

; =============================================================
; redraw_entries: redessinne les 2 lignes du menu
; =============================================================
redraw_entries:
    cmp byte [sel], 0
    je .myos_sel

    ; sel=1: reboot selectionne
    mov bh, 15
    mov bl, 0
    mov si, str_myos
    mov ah, 0x07       ; normal
    call vga_str

    mov bh, 16
    mov bl, 0
    mov si, str_reboot
    mov ah, 0x70       ; inverse (blanc sur noir -> semble selectionne)
    call vga_str
    ret

.myos_sel:
    ; sel=0: MyOS selectionne
    mov bh, 15
    mov bl, 0
    mov si, str_myos
    mov ah, 0x70       ; inverse = selectionne
    call vga_str

    mov bh, 16
    mov bl, 0
    mov si, str_reboot
    mov ah, 0x07       ; normal
    call vga_str
    ret

; =============================================================
; draw_footer: aide + countdown (rows 19-21)
; =============================================================
draw_footer:
    mov bh, 19
    mov bl, 0
    mov si, str_keys
    mov ah, 0x08       ; gris fonce
    call vga_str

    call redraw_countdown
    ret

; =============================================================
; redraw_countdown: met a jour la ligne du countdown (row 21)
; =============================================================
redraw_countdown:
    mov bh, 21
    mov bl, 0
    mov si, str_auto
    mov ah, 0x0E       ; jaune
    call vga_str

    ; Ecrit le chiffre du countdown a la position du '_' (col 30)
    movzx ax, byte [countdown]
    mov bl, 30
    mov bh, 21
    mov si, digits
    add si, ax
    lodsb
    mov ah, 0x0E
    call vga_putc
    ret

; =============================================================
; menu_loop: gestion clavier + countdown
; =============================================================
menu_loop:
    ; Sauvegarde le tick initial
    mov ah, 0x00
    int 0x1A           ; CX:DX = ticks (18.2/sec)
    mov [tick_prev], dx

.loop:
    ; Verifie s'il y a une touche
    mov ah, 0x01
    int 0x16
    jz .check_time

    ; Lit la touche
    mov ah, 0x00
    int 0x16

    ; Entree (0x0D) ou espace = boot
    cmp al, 0x0D
    je .boot_now
    cmp al, ' '
    je .boot_now

    ; Fleche haut (scan=0x48) ou bas (0x50)
    cmp ah, 0x48       ; fleche haut
    je .up
    cmp ah, 0x50       ; fleche bas
    je .down
    jmp .loop

.up:
    mov byte [sel], 0
    call redraw_entries
    mov byte [countdown], TIMEOUT
    call redraw_countdown
    jmp .loop

.down:
    mov byte [sel], 1
    call redraw_entries
    mov byte [countdown], TIMEOUT
    call redraw_countdown
    jmp .loop

.check_time:
    ; Lit les ticks BIOS (18.2 ticks/sec)
    mov ah, 0x00
    int 0x1A
    ; Environ 18 ticks = 1 seconde (on utilise 18)
    mov ax, dx
    sub ax, [tick_prev]
    cmp ax, 18
    jl .loop

    ; 1 seconde passee
    mov ax, dx
    mov [tick_prev], ax

    movzx ax, byte [countdown]
    test ax, ax
    jz .boot_now

    dec byte [countdown]
    call redraw_countdown
    jmp .loop

.boot_now:
    cmp byte [sel], 1
    je .do_reboot

    ; Lance MyOS
    call show_loading
    call detect_memory
    call load_kernel
    call setup_vesa       ; active VESA 640x480x24bpp
    call setup_font       ; copie police BIOS 8x8 → 0x6000
    call show_vesa_status
    call enter_pm         ; ne revient pas

.do_reboot:
    mov al, 0xFE
    out 0x64, al
    jmp $

; =============================================================
; show_loading: affiche message de chargement (row 23)
; =============================================================
show_loading:
    mov bh, 23
    mov bl, 28
    mov si, str_loading
    mov ah, 0x0F       ; blanc vif
    call vga_str
    ret

; =============================================================
; load_kernel: charge KERNEL_SECTS secteurs en CHS (AH=02h)
; 96 secteurs * 512 = 49KB depuis 0x10000 => reste sous 0x20000 (pas de DMA overflow)
; =============================================================
load_kernel:
    ; Chunk 1: 128 sectors from CHS(0,0,18) -> ES=0x1000, BX=0 (physical 0x10000)
    mov ax, KERNEL_SEG
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, KERNEL_SECTS
    mov ch, 0
    mov cl, KERNEL_CHS_S
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jnc .chunk2

    mov bh, 23
    mov bl, 20
    mov si, str_diskerr
    mov ah, 0x0C
    call vga_str
    mov ah, 0x00
    int 0x16
    jmp $

.chunk2:
    ; Chunk 2: 128 sectors from CHS(4,0,2) -> ES=0x2000, BX=0 (physical 0x20000)
    ; LBA 145 = cylinder 4, head 0, sector 2
    mov ax, 0x2000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 128
    mov ch, 4          ; cylinder 4
    mov cl, 2          ; sector 2
    mov dh, 0          ; head 0
    mov dl, [boot_drive]
    int 0x13
    ; if second chunk fails, just continue (may be empty)

.ok:
    ret

; =============================================================
; setup_vesa: active mode graphique 640x480x24bpp via BIOS VBE
; Stocke les parametres a BOOT_INFO (0x0500) pour le kernel
;
; Layout BOOT_INFO:
;   +0 (dword) : adresse physique du framebuffer
;   +4 (word)  : largeur en pixels
;   +6 (word)  : hauteur en pixels
;   +8 (byte)  : bits par pixel
;   +9 (byte)  : 1 = VESA actif, 0 = fallback VGA texte
; =============================================================
setup_vesa:
    ; ES:DI = buffer pour les infos du mode VESA (256 octets)
    xor ax, ax
    mov es, ax
    mov di, VESA_BUF

    ; Interroge le mode 0x0112 (640x480x24bpp)
    mov ax, 0x4F01
    mov cx, 0x0112
    int 0x10
    cmp ax, 0x004F
    jne .fallback

    ; Lit PhysBasePtr (adresse LFB) a l'offset 40 du bloc mode info
    mov eax, dword [VESA_BUF + 40]
    test eax, eax
    jz .fallback

    ; Sauvegarde l'adresse du FB en memoire (int 0x10 peut ecraser EAX)
    mov dword [BOOT_INFO + 0], eax

    ; Active le mode avec le bit Linear Framebuffer (bit 14 = 0x4000)
    mov ax, 0x4F02
    mov bx, 0x4112
    int 0x10
    cmp ax, 0x004F          ; verifie le code de retour AVANT tout pop
    jne .fallback

    ; BOOT_INFO+0 deja rempli, complete le reste
    mov word  [BOOT_INFO + 4], 640   ; largeur
    mov word  [BOOT_INFO + 6], 480   ; hauteur
    mov byte  [BOOT_INFO + 8], 24    ; bpp
    mov byte  [BOOT_INFO + 9], 1     ; flag VESA actif
    ret

.fallback:
    mov byte [BOOT_INFO + 9], 0      ; flag: VGA texte
    ret

; =============================================================
; show_vesa_status: affiche sur la ligne 24 le mode active
; =============================================================
show_vesa_status:
    cmp byte [BOOT_INFO + 9], 1
    je .vesa_ok
    mov bh, 24
    mov bl, 26
    mov si, str_vesa_no
    mov ah, 0x0E
    call vga_str
    ret
.vesa_ok:
    mov bh, 24
    mov bl, 22
    mov si, str_vesa_ok
    mov ah, 0x0A
    call vga_str
    ret

str_vesa_ok db '[Mode VESA 640x480x24bpp OK]', 0
str_vesa_no db '[Fallback: VGA texte 80x25]', 0

; =============================================================
; setup_font: copie la police BIOS 8x8 (256 glyphes x 8 octets)
; de l'espace d'adressage reel vers 0x6000 (lineaire)
; Stocke l'adresse 0x6000 a BOOT_INFO+10 pour le kernel
; =============================================================
setup_font:
    ; int 10h, ax=1130h, bh=6 → ES:BP = police 8x8 BIOS
    mov ax, 0x1130
    mov bh, 6
    int 0x10
    ; ES = segment de la police, BP = offset

    mov si, bp             ; SI = offset source

    ; Copie DS:SI (segment ES) → ES:DI (0x0000:0x6000)
    push es                ; sauvegarde segment source
    pop ds                 ; DS = segment source de la police

    xor ax, ax
    mov es, ax             ; ES = 0 (destination)
    mov di, 0x6000
    mov cx, 2048           ; 256 chars * 8 octets = 2048
    cld
    rep movsb              ; DS:SI → ES:DI

    ; Remet DS = ES = 0
    xor ax, ax
    mov ds, ax

    ; Stocke l'adresse de la police pour le kernel
    mov dword [BOOT_INFO + 10], 0x6000
    ret

; =============================================================
; detect_memory: detecte la RAM et stocke dans BOOT_INFO
; =============================================================
detect_memory:
    mov al, [boot_drive]
    mov [BOOT_INFO_DRIVE], al

    int 0x12
    mov [BOOT_INFO_MEMLO], ax

    mov ah, 0x88
    int 0x15
    jc .dm_done
    mov [BOOT_INFO_MEMHI], ax
.dm_done:
    ret

; =============================================================
; GDT (alignee pour lgdt)
; =============================================================
align 4
gdt_start:
    dq 0x0000000000000000   ; null
    dq 0x00CF9A000000FFFF   ; code 32-bit, ring0, 4GB
    dq 0x00CF92000000FFFF   ; data 32-bit, ring0, 4GB
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; =============================================================
; enter_pm: passe en 32-bit et saute au kernel a 0x10000
; =============================================================
enter_pm:
    cli
    lgdt [gdt_desc]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp 0x08:pm32

[BITS 32]
pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    jmp 0x00010000

[BITS 16]

; =============================================================
; Padding pour aligner sur 16 secteurs (8192 octets)
; =============================================================
times (16*512)-($-$$) db 0
