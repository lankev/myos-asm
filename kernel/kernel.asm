[BITS 32]
[ORG 0x10000]

; Constantes hardware
VGA_MEM  equ 0xB8000
VGA_W    equ 80
VGA_H    equ 25
PIC1_CMD equ 0x20
PIC1_DAT equ 0x21
PIC2_CMD equ 0xA0
PIC2_DAT equ 0xA1
PIT_CH0  equ 0x40
PIT_CMD  equ 0x43
KBD_PORT equ 0x60
KBUF_SZ  equ 64
CMD_LEN  equ 128

; Attributs VGA texte
A_DESK   equ 0x17
A_MENU   equ 0x70
A_STAT   equ 0x70
A_BORD   equ 0x0F
A_TITLE  equ 0x70
A_OUT    equ 0x07
A_GREEN  equ 0x0A
A_CYAN   equ 0x0B
A_RED    equ 0x0C
A_WHITE  equ 0x0F

; --- VESA / Framebuffer ---
BOOT_INFO_ADDR equ 0x0500   ; bloc info MineGRUB→kernel
VESA_PITCH     equ 1920     ; 640 * 3 octets/pixel
; Couleurs 0x00RRGGBB
C_DESKTOP  equ 0x003A6EA5   ; bleu bureau Windows XP
C_TASKBAR  equ 0x001C4F8C   ; bleu taskbar
C_WINBAR   equ 0x000054A0   ; barre de titre active
C_WINCLI   equ 0x00D4D0C8   ; zone client (gris XP)
C_WINBRD   equ 0x00404040   ; bordure fenetre
C_WHITE    equ 0x00FFFFFF
C_BLACK    equ 0x00000000
C_STARTBTN equ 0x003D8C00   ; bouton Start vert
C_CLOSEBTN equ 0x00C00000   ; bouton fermer rouge
C_TERMBG   equ 0x00000000   ; fond terminal
C_TERMFG   equ 0x00C8C8C8   ; texte terminal gris clair
C_TERMGRN  equ 0x0000C800   ; texte vert terminal
; Geometrie du bureau (640x480)
WIN_X      equ 60
WIN_Y      equ 38
WIN_W      equ 520
WIN_H      equ 374
WIN_TH     equ 22           ; title bar height
WIN_MH     equ 20           ; menu bar height
WIN_TX     equ WIN_X + 2
WIN_TY     equ WIN_Y + WIN_TH + WIN_MH
WIN_TW     equ WIN_W - 4
WIN_THGT   equ WIN_H - WIN_TH - WIN_MH - 2
TBAR_Y     equ 450
TBAR_H     equ 30

; Box drawing CP437
C_TL equ 0xC9
C_TR equ 0xBB
C_BL equ 0xC8
C_BR equ 0xBC
C_BH equ 0xCD
C_BV equ 0xBA

; ===== POINT D'ENTREE =====
kernel_start:
    call    init_vars
    call    vesa_init      ; lit BOOT_INFO, init fb si VESA actif
    call    idt_init
    call    pic_init
    call    pit_init
    sti
    cmp     byte [vesa_active], 1
    je      .gui
    call    draw_desktop   ; mode VGA texte classique
    jmp     .shell
.gui:
    call    draw_vesa_desktop
.shell:
    call    shell_run
.halt:
    cli
    hlt
    jmp     .halt

; Initialise les variables globales (car BSS n'est pas garantie a 0 en binaire flat)
init_vars:
    mov dword [vga_col],  0
    mov dword [vga_row],  2
    mov byte  [vga_attr], A_OUT
    mov dword [ticks],    0
    mov dword [kbd_head], 0
    mov dword [kbd_tail], 0
    mov byte  [kbd_sft],  0
    mov byte  [kbd_cap],  0
    ret

; ===== VGA =====

; Ecrit AL a (ECX=col, EDX=row) avec attribut C_BL
wchar:
    push    edi
    push    eax
    mov     edi, edx
    imul    edi, VGA_W
    add     edi, ecx
    shl     edi, 1
    add     edi, VGA_MEM
    mov     [edi],   al
    mov     [edi+1], bl
    pop     eax
    pop     edi
    ret

; Efface l'ecran avec couleur desktop (bleu)
vga_cls:
    pushad
    mov     edi, VGA_MEM
    mov     ecx, VGA_W * VGA_H
    mov     ax,  (A_DESK << 8) | ' '
    rep     stosw
    popad
    ret

; Met a jour le curseur HW a la position [vga_col],[vga_row]
vga_cursor:
    push    eax
    push    ebx
    push    edx
    mov     eax, [vga_row]
    imul    eax, VGA_W
    add     eax, [vga_col]
    mov     ebx, eax
    mov     dx,  0x3D4
    mov     al,  0x0F
    out     dx,  al
    inc     dx
    mov     al,  bl
    out     dx,  al
    dec     dx
    mov     al,  0x0E
    out     dx,  al
    inc     dx
    mov     al,  bh
    out     dx,  al
    pop     edx
    pop     ebx
    pop     eax
    ret

; Active curseur
cursor_on:
    push    eax
    push    edx
    mov     dx,  0x3D4
    mov     al,  0x0A
    out     dx,  al
    inc     dx
    mov     al,  0x0D
    out     dx,  al
    dec     dx
    mov     al,  0x0B
    out     dx,  al
    inc     dx
    mov     al,  0x0E
    out     dx,  al
    pop     edx
    pop     eax
    ret

; Scroll zone 2..VGA_H-3 d'une ligne vers le haut
vga_scroll:
    pushad
    mov     esi, VGA_MEM + VGA_W*2*3
    mov     edi, VGA_MEM + VGA_W*2*2
    mov     ecx, VGA_W * (VGA_H-5)
    rep     movsw
    ; Efface derniere ligne utile (VGA_H-3)
    mov     edi, VGA_MEM + VGA_W*2*(VGA_H-3)
    mov     ecx, VGA_W
    mov     ax,  (A_OUT << 8) | ' '
    rep     stosw
    mov     dword [vga_row], VGA_H-3
    popad
    ret

; Affiche AL dans le terminal (VGA texte ou VESA selon mode actif)
vga_putc:
    cmp     byte [vesa_active], 1
    je      vesa_putc      ; redirect VESA
    push    ebx
    push    ecx
    push    edx
    cmp     al, 10
    je      .nl
    cmp     al, 8
    je      .bs

    ; Ecrit le caractere
    mov     ecx, [vga_col]
    mov     edx, [vga_row]
    movzx   ebx, byte [vga_attr]
    call    wchar
    inc     dword [vga_col]
    cmp     dword [vga_col], VGA_W
    jl      .cur
    mov     dword [vga_col], 0
    inc     dword [vga_row]
    jmp     .sc

.nl:
    mov     dword [vga_col], 0
    inc     dword [vga_row]
    jmp     .sc

.bs:
    cmp     dword [vga_col], 1
    jle     .cur
    dec     dword [vga_col]
    mov     ecx, [vga_col]
    mov     edx, [vga_row]
    movzx   ebx, byte [vga_attr]
    mov     al, ' '
    call    wchar
    jmp     .cur

.sc:
    cmp     dword [vga_row], VGA_H-2
    jl      .cur
    call    vga_scroll

.cur:
    call    vga_cursor
    pop     edx
    pop     ecx
    pop     ebx
    ret

; Affiche string ESI
vga_puts:
    push    eax
    push    esi
.l: lodsb
    test    al, al
    jz      .d
    call    vga_putc
    jmp     .l
.d: pop     esi
    pop     eax
    ret

; Affiche entier EAX (decimal)
vga_putn:
    pushad
    sub     esp, 12
    lea     edi, [esp+11]
    mov     byte [edi], 0
    mov     ebx, 10
    test    eax, eax
    jnz     .c
    dec     edi
    mov     byte [edi], '0'
    jmp     .p
.c: test    eax, eax
    jz      .p
    xor     edx, edx
    div     ebx
    add     dl, '0'
    dec     edi
    mov     [edi], dl
    jmp     .c
.p: mov     esi, edi
    call    vga_puts
    add     esp, 12
    popad
    ret

; Ecrit string ESI a (ECX,EDX) avec attr C_BL
wstr:
    push    eax
    push    ecx
    push    esi
.l: mov     al, [esi]
    test    al, al
    jz      .d
    call    wchar
    inc     ecx
    inc     esi
    jmp     .l
.d: pop     esi
    pop     ecx
    pop     eax
    ret

; Remplit ESI chars de AL/C_BL a (ECX,EDX)
wfill:
    push    ecx
    push    esi
.l: test    esi, esi
    jz      .d
    call    wchar
    inc     ecx
    dec     esi
    jmp     .l
.d: pop     esi
    pop     ecx
    ret

; ===== INTERFACE TUI =====

; Dessine le bureau complet
draw_desktop:
    pushad
    call    vga_cls
    call    draw_menubar
    call    draw_statusbar
    call    draw_winframe
    call    cursor_on
    ; Position curseur dans la fenetre
    mov     dword [vga_col], 1
    mov     dword [vga_row], 2
    ; Message de bienvenue
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_welcome
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    mov     esi, s_hint
    call    vga_puts
    popad
    ret

; Barre de menu ligne 0
draw_menubar:
    pushad
    ; Fond gris toute la ligne
    mov     ecx, 0
    mov     edx, 0
    mov     al,  ' '
    mov     bl,  A_MENU
    mov     esi, VGA_W
    call    wfill
    ; Titre
    mov     ecx, 1
    mov     edx, 0
    mov     bl,  A_MENU
    mov     esi, s_mtitle
    call    wstr
    ; Items
    mov     ecx, 18
    mov     esi, s_mfich
    call    wstr
    mov     ecx, 28
    mov     esi, s_moutils
    call    wstr
    mov     ecx, 38
    mov     esi, s_maide
    call    wstr
    ; Label uptime
    mov     ecx, 65
    mov     esi, s_uplab
    call    wstr
    ; Valeur uptime
    mov     ecx, 69
    mov     eax, [ticks]
    mov     ebx, 100
    xor     edx, edx
    div     ebx
    ; Affiche dans la barre (mode direct)
    push    eax
    mov     dword [vga_col], 69
    mov     dword [vga_row], 0
    mov     byte  [vga_attr], A_MENU
    pop     eax
    call    vga_putn
    popad
    ret

; Barre de statut ligne VGA_H-1
draw_statusbar:
    pushad
    mov     ecx, 0
    mov     edx, VGA_H-1
    mov     al,  ' '
    mov     bl,  A_STAT
    mov     esi, VGA_W
    call    wfill
    mov     ecx, 1
    mov     edx, VGA_H-1
    mov     bl,  A_STAT
    mov     esi, s_status
    call    wstr
    popad
    ret

; Cadre de la fenetre principale (ligne 1 a VGA_H-2)
draw_winframe:
    pushad
    ; Coin C_TL
    mov     ecx, 0
    mov     edx, 1
    mov     al,  C_TL
    mov     bl,  A_BORD
    call    wchar
    ; Bord haut
    mov     ecx, 1
    mov     edx, 1
.th:
    cmp     ecx, VGA_W-1
    jge     .th_done
    mov     al,  C_BH
    call    wchar
    inc     ecx
    jmp     .th
.th_done:
    ; Coin C_TR
    mov     ecx, VGA_W-1
    mov     al,  C_TR
    call    wchar
    ; Titre de la fenetre dans le bord haut
    mov     ecx, 3
    mov     edx, 1
    mov     bl,  A_TITLE
    mov     esi, s_wintitle
    call    wstr

    ; Cotes gauche/droit + interieur blanc
    mov     edx, 2
.sides:
    cmp     edx, VGA_H-2
    jge     .sides_done
    ; Gauche
    mov     ecx, 0
    mov     al,  C_BV
    mov     bl,  A_BORD
    call    wchar
    ; Interieur
    mov     ecx, 1
    mov     al,  ' '
    mov     bl,  A_OUT
    mov     esi, VGA_W-2
    call    wfill
    ; Droite
    mov     ecx, VGA_W-1
    mov     al,  C_BV
    mov     bl,  A_BORD
    call    wchar
    inc     edx
    jmp     .sides
.sides_done:

    ; Bord bas
    mov     edx, VGA_H-2
    mov     ecx, 0
    mov     al,  C_BL
    mov     bl,  A_BORD
    call    wchar
    mov     ecx, 1
.bh:
    cmp     ecx, VGA_W-1
    jge     .bh_done
    mov     al,  C_BH
    call    wchar
    inc     ecx
    jmp     .bh
.bh_done:
    mov     ecx, VGA_W-1
    mov     al,  C_BR
    call    wchar
    popad
    ret

; Raffraichit l'uptime dans la barre de menu
refresh_uptime:
    pushad
    ; Efface les chiffres anciens (5 chars)
    mov     ecx, 69
    mov     edx, 0
    mov     al,  ' '
    mov     bl,  A_MENU
    mov     esi, 5
    call    wfill
    ; Ecrit la nouvelle valeur
    mov     dword [vga_col], 69
    mov     dword [vga_row], 0
    mov     byte  [vga_attr], A_MENU
    mov     eax,  [ticks]
    mov     ebx,  100
    xor     edx,  edx
    div     ebx
    call    vga_putn
    ; Restaure position curseur
    mov     eax, [saved_row]
    mov     [vga_row], eax
    mov     eax, [saved_col]
    mov     [vga_col], eax
    mov     byte [vga_attr], A_OUT
    call    vga_cursor
    popad
    ret

; ===== IDT =====
idt_init:
    mov     ecx, 0
.l: mov     ebx, ecx
    mov     edx, isr_def
    call    idt_set
    inc     ecx
    cmp     ecx, 32
    jl      .l
    mov     ebx, 32
    mov     edx, irq0
    call    idt_set
    mov     ebx, 33
    mov     edx, irq1
    call    idt_set
    mov     ecx, 34
.l2:
    mov     ebx, ecx
    mov     edx, irq_nop
    call    idt_set
    inc     ecx
    cmp     ecx, 48
    jl      .l2
    lidt    [idt_ptr]
    ret

idt_set:
    push    eax
    push    edi
    mov     edi, idt_table
    mov     eax, ebx
    shl     eax, 3
    add     edi, eax
    mov     eax, edx
    mov     [edi+0], ax
    mov     word [edi+2], 0x08
    mov     byte [edi+4], 0
    mov     byte [edi+5], 0x8E
    shr     eax, 16
    mov     [edi+6], ax
    pop     edi
    pop     eax
    ret

isr_def:
    cli
    mov     byte [vga_attr], A_RED
    mov     dword [vga_col], 1
    mov     dword [vga_row], 12
    mov     esi, s_exc
    call    vga_puts
.h: hlt
    jmp     .h

; ===== PIC =====
pic_init:
    mov al, 0x11
    out PIC1_CMD, al
    out PIC2_CMD, al
    mov al, 0x20
    out PIC1_DAT, al
    mov al, 0x28
    out PIC2_DAT, al
    mov al, 0x04
    out PIC1_DAT, al
    mov al, 0x02
    out PIC2_DAT, al
    mov al, 0x01
    out PIC1_DAT, al
    out PIC2_DAT, al
    mov al, 11111100b
    out PIC1_DAT, al
    mov al, 11111111b
    out PIC2_DAT, al
    ret

pic_eoi:
    mov al, 0x20
    out PIC1_CMD, al
    ret

; ===== PIT =====
pit_init:
    mov al, 0x36
    out PIT_CMD, al
    mov ax, 11931
    out PIT_CH0, al
    mov al, ah
    out PIT_CH0, al
    ret

irq0:
    inc     dword [ticks]
    mov     eax, [ticks]
    and     eax, 127
    test    eax, eax
    jnz     .e
    call    refresh_uptime
.e: call    pic_eoi
    iret

irq_nop:
    call    pic_eoi
    iret

; ===== CLAVIER =====
sc_lo:
    db 0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9
    db 'q','w','e','r','t','y','u','i','o','p','[',']',13,0
    db 'a','s','d','f','g','h','j','k','l',';',39,'`',0,92
    db 'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
sc_hi:
    db 0,27,'!','@','#','$','%','^','&','*','(',')',95,'+',8,9
    db 'Q','W','E','R','T','Y','U','I','O','P','{','}',13,0
    db 'A','S','D','F','G','H','J','K','L',':',34,'~',0,'|'
    db 'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '

irq1:
    push    eax
    push    ebx
    in      al, KBD_PORT
    cmp     al, 0x2A
    je      .s1
    cmp     al, 0x36
    je      .s1
    cmp     al, 0xAA
    je      .s0
    cmp     al, 0xB6
    je      .s0
    cmp     al, 0x3A
    je      .cap
    test    al, 0x80
    jnz     .eoi
    cmp     al, 54
    ja      .eoi
    movzx   ebx, al
    cmp     byte [kbd_sft], 0
    jne     .hi
    mov     al, [sc_lo+ebx]
    jmp     .got
.hi:
    mov     al, [sc_hi+ebx]
.got:
    test    al, al
    jz      .eoi
    cmp     byte [kbd_cap], 0
    je      .sto
    cmp     al, 'a'
    jb      .sto
    cmp     al, 'z'
    ja      .sto
    sub     al, 32
.sto:
    mov     ebx, [kbd_head]
    mov     [kbd_buf+ebx], al
    inc     ebx
    and     ebx, KBUF_SZ-1
    cmp     ebx, [kbd_tail]
    je      .eoi
    mov     [kbd_head], ebx
    jmp     .eoi
.s1: mov    byte [kbd_sft], 1
    jmp     .eoi
.s0: mov    byte [kbd_sft], 0
    jmp     .eoi
.cap:
    xor     byte [kbd_cap], 1
.eoi:
    call    pic_eoi
    pop     ebx
    pop     eax
    iret

kbd_get:
.w: mov     eax, [kbd_tail]
    cmp     eax, [kbd_head]
    je      .w
    mov     ebx, [kbd_tail]
    mov     al, [kbd_buf+ebx]
    inc     ebx
    and     ebx, KBUF_SZ-1
    mov     [kbd_tail], ebx
    ret

; Lit ligne dans EDI (max ECX chars). EAX=longueur.
kbd_line:
    push    ebx
    push    ecx
    push    edi
    xor     ebx, ebx
.l: call    kbd_get
    cmp     al, 13
    je      .ent
    cmp     al, 8
    je      .bs
    cmp     ebx, ecx
    jge     .l
    mov     [edi+ebx], al
    inc     ebx
    call    vga_putc
    jmp     .l
.bs:
    test    ebx, ebx
    jz      .l
    dec     ebx
    call    vga_putc
    jmp     .l
.ent:
    mov     byte [edi+ebx], 0
    mov     al, 10
    call    vga_putc
    mov     eax, ebx
    pop     edi
    pop     ecx
    pop     ebx
    ret

; ===== SHELL =====

shell_run:
.lp:
    ; Sauvegarde position pour refresh_uptime
    mov     eax, [vga_row]
    mov     [saved_row], eax
    mov     eax, [vga_col]
    mov     [saved_col], eax

    ; Prompt
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_puser
    call    vga_puts
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_ppath
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    mov     esi, s_pend
    call    vga_puts

    ; Lecture
    mov     edi, cmd_buf
    mov     ecx, CMD_LEN-1
    call    kbd_line

    call    dispatch
    jmp     .lp

dispatch:
    cmp     byte [cmd_buf], 0
    je      .done

    mov     esi, cmd_buf
    mov     edi, sc_help
    call    seq
    test    eax, eax
    jnz     .help

    mov     esi, cmd_buf
    mov     edi, sc_clear
    call    seq
    test    eax, eax
    jnz     .clear

    mov     esi, cmd_buf
    mov     edi, sc_echo
    call    spfx
    test    eax, eax
    jnz     .echo

    mov     esi, cmd_buf
    mov     edi, sc_color
    call    spfx
    test    eax, eax
    jnz     .color

    mov     esi, cmd_buf
    mov     edi, sc_up
    call    seq
    test    eax, eax
    jnz     .uptime

    mov     esi, cmd_buf
    mov     edi, sc_uname
    call    seq
    test    eax, eax
    jnz     .uname

    mov     esi, cmd_buf
    mov     edi, sc_gfx
    call    seq
    test    eax, eax
    jnz     .gfx

    mov     esi, cmd_buf
    mov     edi, sc_reboot
    call    seq
    test    eax, eax
    jnz     .reboot

    ; --- whoami ---
    mov     esi, cmd_buf
    mov     edi, sc_whoami
    call    seq
    test    eax, eax
    jnz     .whoami

    ; --- id ---
    mov     esi, cmd_buf
    mov     edi, sc_id
    call    seq
    test    eax, eax
    jnz     .id

    ; --- hostname ---
    mov     esi, cmd_buf
    mov     edi, sc_hostname
    call    seq
    test    eax, eax
    jnz     .hostname

    ; --- arch ---
    mov     esi, cmd_buf
    mov     edi, sc_arch
    call    seq
    test    eax, eax
    jnz     .arch

    ; --- date ---
    mov     esi, cmd_buf
    mov     edi, sc_date
    call    seq
    test    eax, eax
    jnz     .date_cmd

    ; --- pwd ---
    mov     esi, cmd_buf
    mov     edi, sc_pwd
    call    seq
    test    eax, eax
    jnz     .pwd

    ; --- ls / ll / dir ---
    mov     esi, cmd_buf
    mov     edi, sc_ls
    call    seq
    test    eax, eax
    jnz     .ls
    mov     esi, cmd_buf
    mov     edi, sc_ls_pfx
    call    spfx
    test    eax, eax
    jnz     .ls
    mov     esi, cmd_buf
    mov     edi, sc_ll
    call    seq
    test    eax, eax
    jnz     .ls
    mov     esi, cmd_buf
    mov     edi, sc_dir
    call    seq
    test    eax, eax
    jnz     .ls

    ; --- cat specifique ---
    mov     esi, cmd_buf
    mov     edi, sc_cat_cpu
    call    seq
    test    eax, eax
    jnz     .cat_cpu
    mov     esi, cmd_buf
    mov     edi, sc_cat_mem
    call    seq
    test    eax, eax
    jnz     .cat_mem
    mov     esi, cmd_buf
    mov     edi, sc_cat_host
    call    seq
    test    eax, eax
    jnz     .cat_host
    mov     esi, cmd_buf
    mov     edi, sc_cat_osr
    call    seq
    test    eax, eax
    jnz     .cat_osr
    mov     esi, cmd_buf
    mov     edi, sc_cat_ver
    call    seq
    test    eax, eax
    jnz     .cat_ver
    ; generic cat
    mov     esi, cmd_buf
    mov     edi, sc_cat_pfx
    call    spfx
    test    eax, eax
    jnz     .cat_gen

    ; --- ps ---
    mov     esi, cmd_buf
    mov     edi, sc_ps
    call    seq
    test    eax, eax
    jnz     .ps

    ; --- top ---
    mov     esi, cmd_buf
    mov     edi, sc_top
    call    seq
    test    eax, eax
    jnz     .top

    ; --- free ---
    mov     esi, cmd_buf
    mov     edi, sc_free
    call    seq
    test    eax, eax
    jnz     .free

    ; --- df ---
    mov     esi, cmd_buf
    mov     edi, sc_df
    call    seq
    test    eax, eax
    jnz     .df

    ; --- dmesg ---
    mov     esi, cmd_buf
    mov     edi, sc_dmesg
    call    seq
    test    eax, eax
    jnz     .dmesg

    ; --- lscpu ---
    mov     esi, cmd_buf
    mov     edi, sc_lscpu
    call    seq
    test    eax, eax
    jnz     .lscpu

    ; --- lspci ---
    mov     esi, cmd_buf
    mov     edi, sc_lspci
    call    seq
    test    eax, eax
    jnz     .lspci

    ; --- lsblk ---
    mov     esi, cmd_buf
    mov     edi, sc_lsblk
    call    seq
    test    eax, eax
    jnz     .lsblk

    ; --- lsusb ---
    mov     esi, cmd_buf
    mov     edi, sc_lsusb
    call    seq
    test    eax, eax
    jnz     .lsusb

    ; --- lsmod ---
    mov     esi, cmd_buf
    mov     edi, sc_lsmod
    call    seq
    test    eax, eax
    jnz     .lsmod

    ; --- mem ---
    mov     esi, cmd_buf
    mov     edi, sc_mem
    call    seq
    test    eax, eax
    jnz     .mem

    ; --- mount ---
    mov     esi, cmd_buf
    mov     edi, sc_mount
    call    seq
    test    eax, eax
    jnz     .mount

    ; --- env ---
    mov     esi, cmd_buf
    mov     edi, sc_env
    call    seq
    test    eax, eax
    jnz     .env

    ; --- ifconfig / net ---
    mov     esi, cmd_buf
    mov     edi, sc_ifc
    call    seq
    test    eax, eax
    jnz     .ifconfig
    mov     esi, cmd_buf
    mov     edi, sc_net
    call    seq
    test    eax, eax
    jnz     .ifconfig

    ; --- netstat ---
    mov     esi, cmd_buf
    mov     edi, sc_netstat
    call    seq
    test    eax, eax
    jnz     .netstat

    ; --- ping ---
    mov     esi, cmd_buf
    mov     edi, sc_ping
    call    seq
    test    eax, eax
    jnz     .ping
    mov     esi, cmd_buf
    mov     edi, sc_ping_pfx
    call    spfx
    test    eax, eax
    jnz     .ping

    ; --- about ---
    mov     esi, cmd_buf
    mov     edi, sc_about
    call    seq
    test    eax, eax
    jnz     .about

    ; --- fortune ---
    mov     esi, cmd_buf
    mov     edi, sc_fortune
    call    seq
    test    eax, eax
    jnz     .fortune

    ; --- cowsay ---
    mov     esi, cmd_buf
    mov     edi, sc_cowsay
    call    seq
    test    eax, eax
    jnz     .cowsay
    mov     esi, cmd_buf
    mov     edi, sc_cowsay_pfx
    call    spfx
    test    eax, eax
    jnz     .cowsay

    ; --- matrix ---
    mov     esi, cmd_buf
    mov     edi, sc_matrix
    call    seq
    test    eax, eax
    jnz     .matrix

    ; --- sl ---
    mov     esi, cmd_buf
    mov     edi, sc_sl
    call    seq
    test    eax, eax
    jnz     .sl

    ; --- creeper ---
    mov     esi, cmd_buf
    mov     edi, sc_creeper
    call    seq
    test    eax, eax
    jnz     .creeper

    ; --- true ---
    mov     esi, cmd_buf
    mov     edi, sc_true
    call    seq
    test    eax, eax
    jnz     .done

    ; --- false ---
    mov     esi, cmd_buf
    mov     edi, sc_false
    call    seq
    test    eax, eax
    jnz     .cmd_false

    ; --- exit / logout ---
    mov     esi, cmd_buf
    mov     edi, sc_exit
    call    seq
    test    eax, eax
    jnz     .exit
    mov     esi, cmd_buf
    mov     edi, sc_logout
    call    seq
    test    eax, eax
    jnz     .exit

    ; --- shutdown / poweroff / halt ---
    mov     esi, cmd_buf
    mov     edi, sc_shutdown
    call    seq
    test    eax, eax
    jnz     .reboot
    mov     esi, cmd_buf
    mov     edi, sc_poweroff
    call    seq
    test    eax, eax
    jnz     .reboot
    mov     esi, cmd_buf
    mov     edi, sc_halt_cmd
    call    seq
    test    eax, eax
    jnz     .reboot

    ; --- sudo / su ---
    mov     esi, cmd_buf
    mov     edi, sc_sudo
    call    seq
    test    eax, eax
    jnz     .sudo
    mov     esi, cmd_buf
    mov     edi, sc_sudo_pfx
    call    spfx
    test    eax, eax
    jnz     .sudo
    mov     esi, cmd_buf
    mov     edi, sc_su
    call    seq
    test    eax, eax
    jnz     .sudo

    ; --- bash / sh / zsh ---
    mov     esi, cmd_buf
    mov     edi, sc_bash
    call    seq
    test    eax, eax
    jnz     .shell_msg
    mov     esi, cmd_buf
    mov     edi, sc_sh
    call    seq
    test    eax, eax
    jnz     .shell_msg
    mov     esi, cmd_buf
    mov     edi, sc_zsh
    call    seq
    test    eax, eax
    jnz     .shell_msg

    ; --- sync ---
    mov     esi, cmd_buf
    mov     edi, sc_sync
    call    seq
    test    eax, eax
    jnz     .done

    ; --- history ---
    mov     esi, cmd_buf
    mov     edi, sc_history
    call    seq
    test    eax, eax
    jnz     .history

    ; --- cal ---
    mov     esi, cmd_buf
    mov     edi, sc_cal
    call    seq
    test    eax, eax
    jnz     .cal

    ; --- cls (alias clear) ---
    mov     esi, cmd_buf
    mov     edi, sc_cls
    call    seq
    test    eax, eax
    jnz     .clear

    ; --- time ---
    mov     esi, cmd_buf
    mov     edi, sc_time
    call    seq
    test    eax, eax
    jnz     .time_cmd

    ; --- sleep ---
    mov     esi, cmd_buf
    mov     edi, sc_sleep
    call    seq
    test    eax, eax
    jnz     .sleep_cmd
    mov     esi, cmd_buf
    mov     edi, sc_sleep_pfx
    call    spfx
    test    eax, eax
    jnz     .sleep_cmd

    ; --- banner ---
    mov     esi, cmd_buf
    mov     edi, sc_banner
    call    seq
    test    eax, eax
    jnz     .banner_gen
    mov     esi, cmd_buf
    mov     edi, sc_banner_pfx
    call    spfx
    test    eax, eax
    jnz     .banner_gen

    ; --- man / info ---
    mov     esi, cmd_buf
    mov     edi, sc_man_pfx
    call    spfx
    test    eax, eax
    jnz     .man_cmd

    ; --- which ---
    mov     esi, cmd_buf
    mov     edi, sc_which_pfx
    call    spfx
    test    eax, eax
    jnz     .which_cmd

    ; --- kill / killall ---
    mov     esi, cmd_buf
    mov     edi, sc_kill_pfx
    call    spfx
    test    eax, eax
    jnz     .kill_cmd

    ; --- mkdir ---
    mov     esi, cmd_buf
    mov     edi, sc_mkdir_pfx
    call    spfx
    test    eax, eax
    jnz     .mkdir_cmd

    ; --- touch ---
    mov     esi, cmd_buf
    mov     edi, sc_touch_pfx
    call    spfx
    test    eax, eax
    jnz     .touch_cmd

    ; --- rm / rmdir ---
    mov     esi, cmd_buf
    mov     edi, sc_rm_pfx
    call    spfx
    test    eax, eax
    jnz     .rm_cmd

    ; --- cp ---
    mov     esi, cmd_buf
    mov     edi, sc_cp_pfx
    call    spfx
    test    eax, eax
    jnz     .cp_cmd

    ; --- mv ---
    mov     esi, cmd_buf
    mov     edi, sc_mv_pfx
    call    spfx
    test    eax, eax
    jnz     .mv_cmd

    ; --- chmod / chown ---
    mov     esi, cmd_buf
    mov     edi, sc_chmod_pfx
    call    spfx
    test    eax, eax
    jnz     .done
    mov     esi, cmd_buf
    mov     edi, sc_chown_pfx
    call    spfx
    test    eax, eax
    jnz     .done

    ; --- grep ---
    mov     esi, cmd_buf
    mov     edi, sc_grep_pfx
    call    spfx
    test    eax, eax
    jnz     .grep_cmd
    mov     esi, cmd_buf
    mov     edi, sc_grep
    call    seq
    test    eax, eax
    jnz     .grep_cmd

    ; --- find ---
    mov     esi, cmd_buf
    mov     edi, sc_find_pfx
    call    spfx
    test    eax, eax
    jnz     .find_cmd
    mov     esi, cmd_buf
    mov     edi, sc_find
    call    seq
    test    eax, eax
    jnz     .find_cmd

    ; --- du ---
    mov     esi, cmd_buf
    mov     edi, sc_du_pfx
    call    spfx
    test    eax, eax
    jnz     .du_cmd
    mov     esi, cmd_buf
    mov     edi, sc_du
    call    seq
    test    eax, eax
    jnz     .du_cmd

    ; --- stat ---
    mov     esi, cmd_buf
    mov     edi, sc_stat_pfx
    call    spfx
    test    eax, eax
    jnz     .stat_cmd

    ; --- file ---
    mov     esi, cmd_buf
    mov     edi, sc_file_pfx
    call    spfx
    test    eax, eax
    jnz     .file_cmd

    ; --- strace / ltrace / gdb / valgrind ---
    mov     esi, cmd_buf
    mov     edi, sc_strace_pfx
    call    spfx
    test    eax, eax
    jnz     .nosupport
    mov     esi, cmd_buf
    mov     edi, sc_gdb_pfx
    call    spfx
    test    eax, eax
    jnz     .nosupport

    ; --- make / gcc / nasm ---
    mov     esi, cmd_buf
    mov     edi, sc_make_pfx
    call    spfx
    test    eax, eax
    jnz     .nosupport2
    mov     esi, cmd_buf
    mov     edi, sc_gcc_pfx
    call    spfx
    test    eax, eax
    jnz     .nosupport2

    ; --- curl / wget ---
    mov     esi, cmd_buf
    mov     edi, sc_curl_pfx
    call    spfx
    test    eax, eax
    jnz     .nosupport3
    mov     esi, cmd_buf
    mov     edi, sc_wget_pfx
    call    spfx
    test    eax, eax
    jnz     .nosupport3

    ; --- ssh / ftp ---
    mov     esi, cmd_buf
    mov     edi, sc_ssh_pfx
    call    spfx
    test    eax, eax
    jnz     .nosupport4

    ; --- echo sans args ---
    mov     esi, cmd_buf
    mov     edi, sc_echo_bare
    call    seq
    test    eax, eax
    jnz     .echo_nl

    ; --- head / tail ---
    mov     esi, cmd_buf
    mov     edi, sc_head_pfx
    call    spfx
    test    eax, eax
    jnz     .cat_gen
    mov     esi, cmd_buf
    mov     edi, sc_tail_pfx
    call    spfx
    test    eax, eax
    jnz     .cat_gen

    ; --- wc ---
    mov     esi, cmd_buf
    mov     edi, sc_wc_pfx
    call    spfx
    test    eax, eax
    jnz     .wc_cmd

    ; --- sort / uniq / cut / tr / sed / awk ---
    mov     esi, cmd_buf
    mov     edi, sc_sort_pfx
    call    spfx
    test    eax, eax
    jnz     .stdin_cmd
    mov     esi, cmd_buf
    mov     edi, sc_sed_pfx
    call    spfx
    test    eax, eax
    jnz     .stdin_cmd
    mov     esi, cmd_buf
    mov     edi, sc_awk_pfx
    call    spfx
    test    eax, eax
    jnz     .stdin_cmd

    ; Inconnu
    mov     byte [vga_attr], A_RED
    mov     esi, s_unk
    call    vga_puts
    mov     esi, cmd_buf
    call    vga_puts
    mov     al, 10
    call    vga_putc
    mov     byte [vga_attr], A_OUT
    jmp     .done

.help:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_help
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.clear:
    call    draw_desktop
    jmp     .done

.echo:
    mov     byte [vga_attr], A_WHITE
    lea     esi, [cmd_buf+5]
    call    vga_puts
    mov     al, 10
    call    vga_putc
    mov     byte [vga_attr], A_OUT
    jmp     .done

.color:
    movzx   eax, byte [cmd_buf+6]
    sub     al, '0'
    cmp     al, 9
    jbe     .sc
    movzx   eax, byte [cmd_buf+6]
    sub     al, '0'
    imul    eax, 10
    movzx   ebx, byte [cmd_buf+7]
    sub     bl, '0'
    add     al, bl
.sc:
    mov     [vga_attr], al
    mov     esi, s_colok
    call    vga_puts
    jmp     .done

.uptime:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_upt
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    mov     eax, [ticks]
    mov     ebx, 100
    xor     edx, edx
    div     ebx
    call    vga_putn
    mov     esi, s_sec
    call    vga_puts
    jmp     .done

.uname:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_uname
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.gfx:
    call    demo_gfx
    jmp     .done

.reboot:
    mov     byte [vga_attr], A_RED
    mov     esi, s_rbt
    call    vga_puts
    mov     ecx, 0x800000
.w: loop    .w
    mov     al,  0xFE
    out     0x64, al
    jmp     $

.whoami:
    mov     esi, s_whoami
    call    vga_puts
    jmp     .done

.id:
    mov     esi, s_id
    call    vga_puts
    jmp     .done

.hostname:
    mov     esi, s_hostname_out
    call    vga_puts
    jmp     .done

.arch:
    mov     esi, s_arch
    call    vga_puts
    jmp     .done

.date_cmd:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_date
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.pwd:
    mov     esi, s_pwd
    call    vga_puts
    jmp     .done

.ls:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_ls
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.cat_cpu:
    mov     esi, s_cat_cpu
    call    vga_puts
    jmp     .done

.cat_mem:
    mov     esi, s_cat_mem
    call    vga_puts
    jmp     .done

.cat_host:
    mov     esi, s_hostname_out
    call    vga_puts
    jmp     .done

.cat_osr:
    mov     esi, s_cat_osr
    call    vga_puts
    jmp     .done

.cat_ver:
    mov     esi, s_cat_ver
    call    vga_puts
    jmp     .done

.cat_gen:
    mov     esi, s_cat_gen
    call    vga_puts
    jmp     .done

.ps:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_ps
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.top:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_top
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.free:
    mov     esi, s_free_out
    call    vga_puts
    jmp     .done

.df:
    mov     esi, s_df
    call    vga_puts
    jmp     .done

.dmesg:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_dmesg
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.lscpu:
    mov     esi, s_lscpu
    call    vga_puts
    jmp     .done

.lspci:
    mov     esi, s_lspci
    call    vga_puts
    jmp     .done

.lsblk:
    mov     esi, s_lsblk
    call    vga_puts
    jmp     .done

.lsusb:
    mov     esi, s_lsusb
    call    vga_puts
    jmp     .done

.lsmod:
    mov     esi, s_lsmod
    call    vga_puts
    jmp     .done

.mem:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_mem
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.mount:
    mov     esi, s_mount
    call    vga_puts
    jmp     .done

.env:
    mov     esi, s_env
    call    vga_puts
    jmp     .done

.ifconfig:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_ifconfig
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.netstat:
    mov     esi, s_netstat
    call    vga_puts
    jmp     .done

.ping:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_ping
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.about:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_about
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.fortune:
    mov     eax, [fortune_idx]
    mov     esi, [fort_table + eax*4]
    call    vga_puts
    inc     dword [fortune_idx]
    mov     eax, [fortune_idx]
    cmp     eax, 8
    jl      .done
    mov     dword [fortune_idx], 0
    jmp     .done

.cowsay:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_cowsay
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.matrix:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_matrix
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.sl:
    mov     byte [vga_attr], A_WHITE
    mov     esi, s_sl
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.creeper:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_creeper
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.cmd_false:
    mov     byte [vga_attr], A_RED
    mov     esi, s_false_out
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.exit:
    mov     esi, s_exit
    call    vga_puts
    jmp     .done

.sudo:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_sudo
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.shell_msg:
    mov     esi, s_shell
    call    vga_puts
    jmp     .done

.history:
    mov     esi, s_history
    call    vga_puts
    jmp     .done

.cal:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_cal
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.time_cmd:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_time
    call    vga_puts
    ; affiche ticks comme uptime
    mov     eax, [ticks]
    mov     ebx, 100
    xor     edx, edx
    div     ebx
    call    vga_putn
    mov     esi, s_sec
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.sleep_cmd:
    mov     ecx, 0x2000000
.slp_lp:
    loop    .slp_lp
    jmp     .done

.banner_gen:
    mov     byte [vga_attr], A_WHITE
    mov     esi, s_banner
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.man_cmd:
    mov     esi, s_man
    call    vga_puts
    jmp     .done

.which_cmd:
    mov     esi, s_which
    call    vga_puts
    jmp     .done

.kill_cmd:
    mov     byte [vga_attr], A_RED
    mov     esi, s_kill
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.mkdir_cmd:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_mkdir
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.touch_cmd:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_touch
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.rm_cmd:
    mov     byte [vga_attr], A_RED
    mov     esi, s_rm
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.cp_cmd:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_cp
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.mv_cmd:
    mov     byte [vga_attr], A_GREEN
    mov     esi, s_mv
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.grep_cmd:
    mov     esi, s_grep
    call    vga_puts
    jmp     .done

.find_cmd:
    mov     byte [vga_attr], A_CYAN
    mov     esi, s_find_out
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.du_cmd:
    mov     esi, s_du
    call    vga_puts
    jmp     .done

.stat_cmd:
    mov     esi, s_stat
    call    vga_puts
    jmp     .done

.file_cmd:
    mov     esi, s_file
    call    vga_puts
    jmp     .done

.nosupport:
    mov     byte [vga_attr], A_RED
    mov     esi, s_nosupport
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.nosupport2:
    mov     byte [vga_attr], A_RED
    mov     esi, s_nosupport2
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.nosupport3:
    mov     byte [vga_attr], A_RED
    mov     esi, s_nosupport3
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.nosupport4:
    mov     byte [vga_attr], A_RED
    mov     esi, s_nosupport4
    call    vga_puts
    mov     byte [vga_attr], A_OUT
    jmp     .done

.echo_nl:
    mov     al, 10
    call    vga_putc
    jmp     .done

.wc_cmd:
    mov     esi, s_wc
    call    vga_puts
    jmp     .done

.stdin_cmd:
    mov     esi, s_stdin
    call    vga_puts
    jmp     .done

.done:
    ret

; Demo couleurs : remplit l'ecran avec blocs 0xDB en 16 couleurs
demo_gfx:
    pushad
    call    vga_cls
    mov     edi, VGA_MEM
    mov     ecx, 0
.rl:
    cmp     ecx, VGA_H
    jge     .done
    push    ecx
    xor     ebx, ebx
.cl:
    cmp     ebx, VGA_W
    jge     .ce
    mov     eax, ecx
    add     eax, ebx
    and     eax, 0x0F
    shl     al,  4
    push    eax
    mov     eax, ebx
    and     eax, 0x0F
    or      al,  [esp]
    add     esp, 4
    mov     [edi+1], al
    mov     byte [edi], 0xDB
    add     edi, 2
    inc     ebx
    jmp     .cl
.ce:
    pop     ecx
    inc     ecx
    jmp     .rl
.done:
    call    kbd_get
    call    draw_desktop
    popad
    ret

; ===== UTILITAIRES STRING =====
seq:
    push    esi
    push    edi
    push    ebx
.l: mov     al, [esi]
    mov     bl, [edi]
    cmp     al, bl
    jne     .n
    test    al, al
    jz      .y
    inc     esi
    inc     edi
    jmp     .l
.y: mov     eax, 1
    jmp     .r
.n: xor     eax, eax
.r: pop     ebx
    pop     edi
    pop     esi
    ret

spfx:
    push    esi
    push    edi
    push    ebx
.l: mov     bl, [edi]
    test    bl, bl
    jz      .y
    mov     al, [esi]
    cmp     al, bl
    jne     .n
    inc     esi
    inc     edi
    jmp     .l
.y: mov     eax, 1
    jmp     .r
.n: xor     eax, eax
.r: pop     ebx
    pop     edi
    pop     esi
    ret

; =============================================================
; ===== VESA FRAMEBUFFER =====
; =============================================================

; vesa_init: lit BOOT_INFO (0x0500) et initialise les variables
vesa_init:
    mov     al,  [BOOT_INFO_ADDR + 9]
    mov     [vesa_active], al
    test    al,  al
    jz      .no
    mov     eax, [BOOT_INFO_ADDR + 0]
    mov     [fb_addr],  eax
    mov     eax, [BOOT_INFO_ADDR + 10]
    mov     [vesa_font], eax
    mov     dword [vesa_cur_x], WIN_TX + 4
    mov     dword [vesa_cur_y], WIN_TY + 4
.no:
    ret

; fb_putpixel: eax=0x00RRGGBB, ecx=x, edx=y
fb_putpixel:
    pushad
    cmp     ecx, 640
    jge     .skip
    cmp     edx, 480
    jge     .skip
    mov     edi, [fb_addr]
    push    eax
    mov     eax, edx
    imul    eax, VESA_PITCH
    add     edi, eax
    mov     eax, ecx
    lea     eax, [eax+eax*2]
    add     edi, eax
    pop     eax
    mov     [edi],   al    ; B
    mov     [edi+1], ah    ; G
    shr     eax, 16
    mov     [edi+2], al    ; R
.skip:
    popad
    ret

; fb_fill_rect: eax=color, ecx=x, edx=y, ebx=w, esi=h
fb_fill_rect:
    pushad
    mov     [frc_color], eax
    mov     edi, [fb_addr]
    mov     eax, edx
    imul    eax, VESA_PITCH
    add     edi, eax
    mov     eax, ecx
    lea     eax, [eax+eax*2]
    add     edi, eax
.fr_row:
    test    esi, esi
    jle     .fr_done
    push    edi
    push    esi
    mov     esi, ebx
    mov     eax, [frc_color]
.fr_pix:
    test    esi, esi
    jle     .fr_erow
    mov     [edi],   al
    mov     [edi+1], ah
    push    eax
    shr     eax, 16
    mov     [edi+2], al
    pop     eax
    add     edi, 3
    dec     esi
    jmp     .fr_pix
.fr_erow:
    pop     esi
    pop     edi
    add     edi, VESA_PITCH
    dec     esi
    jmp     .fr_row
.fr_done:
    popad
    ret

; fb_putchar: al=char, ecx=x, edx=y, ebx=fg (0x00RRGGBB), esi=bg
; Rend un caractere 8x8 avec la police BIOS copiee par MineGRUB
fb_putchar:
    pushad
    mov     [fc_fg],  ebx
    mov     [fc_bg],  esi
    mov     [fc_x],   ecx
    mov     [fc_y],   edx
    movzx   eax, al
    imul    eax, 8
    add     eax, [vesa_font]
    mov     esi, eax           ; esi = pointeur glyphe
    mov     dword [fc_row], 0
.fc_row:
    cmp     dword [fc_row], 8
    jge     .fc_done
    mov     ebx, [fc_row]
    movzx   eax, byte [esi+ebx]
    mov     dl,  0x80          ; masque bit MSB → pixel gauche
    mov     dword [fc_col], 0
.fc_col:
    cmp     dword [fc_col], 8
    jge     .fc_nrow
    test    al,  dl
    jz      .fc_bg
    mov     ecx, [fc_fg]
    jmp     .fc_draw
.fc_bg:
    mov     ecx, [fc_bg]
.fc_draw:
    push    eax
    push    edx
    mov     eax, ecx
    mov     ecx, [fc_x]
    add     ecx, [fc_col]
    mov     edx, [fc_y]
    add     edx, [fc_row]
    call    fb_putpixel
    pop     edx
    pop     eax
    shr     dl,  1
    inc     dword [fc_col]
    jmp     .fc_col
.fc_nrow:
    inc     dword [fc_row]
    jmp     .fc_row
.fc_done:
    popad
    ret

; fb_puts: esi=str, ecx=x, edx=y, ebx=fg, edi=bg
fb_puts:
    push    eax
    push    ecx
    push    esi
.fp_l:
    mov     al,  [esi]
    test    al,  al
    jz      .fp_done
    call    fb_putchar
    add     ecx, 8
    inc     esi
    jmp     .fp_l
.fp_done:
    pop     esi
    pop     ecx
    pop     eax
    ret

; draw_vesa_desktop: bureau Windows-like 640x480
draw_vesa_desktop:
    pushad

    ; Fond bureau bleu
    xor     ecx, ecx
    xor     edx, edx
    mov     ebx, 640
    mov     esi, 480
    mov     eax, C_DESKTOP
    call    fb_fill_rect

    ; Taskbar
    xor     ecx, ecx
    mov     edx, TBAR_Y
    mov     ebx, 640
    mov     esi, TBAR_H
    mov     eax, C_TASKBAR
    call    fb_fill_rect
    ; Ligne haute taskbar
    xor     ecx, ecx
    mov     edx, TBAR_Y
    mov     ebx, 640
    mov     esi, 1
    mov     eax, C_WHITE
    call    fb_fill_rect
    ; Bouton Start
    mov     ecx, 4
    mov     edx, TBAR_Y + 4
    mov     ebx, 74
    mov     esi, TBAR_H - 8
    mov     eax, C_STARTBTN
    call    fb_fill_rect
    mov     ecx, 12
    mov     edx, TBAR_Y + 9
    mov     esi, s_start
    mov     ebx, C_WHITE
    mov     edi, C_STARTBTN
    call    fb_puts
    ; Horloge
    mov     ecx, 570
    mov     edx, TBAR_Y + 9
    mov     esi, s_clock
    mov     ebx, C_WHITE
    mov     edi, C_TASKBAR
    call    fb_puts

    ; Ombre fenetre
    mov     ecx, WIN_X + 4
    mov     edx, WIN_Y + 4
    mov     ebx, WIN_W
    mov     esi, WIN_H
    mov     eax, 0x00101010
    call    fb_fill_rect

    ; Bordure fenetre
    mov     ecx, WIN_X
    mov     edx, WIN_Y
    mov     ebx, WIN_W
    mov     esi, WIN_H
    mov     eax, C_WINBRD
    call    fb_fill_rect

    ; Barre de titre
    mov     ecx, WIN_X + 2
    mov     edx, WIN_Y + 2
    mov     ebx, WIN_W - 4
    mov     esi, WIN_TH - 2
    mov     eax, C_WINBAR
    call    fb_fill_rect
    ; Titre
    mov     ecx, WIN_X + 8
    mov     edx, WIN_Y + 6
    mov     esi, s_vesa_title
    mov     ebx, C_WHITE
    mov     edi, C_WINBAR
    call    fb_puts

    ; Bouton X (fermer)
    mov     ecx, WIN_X + WIN_W - 21
    mov     edx, WIN_Y + 3
    mov     ebx, 18
    mov     esi, WIN_TH - 5
    mov     eax, C_CLOSEBTN
    call    fb_fill_rect
    mov     ecx, WIN_X + WIN_W - 17
    mov     edx, WIN_Y + 7
    mov     esi, s_btn_x
    mov     ebx, C_WHITE
    mov     edi, C_CLOSEBTN
    call    fb_puts

    ; Bouton max
    mov     ecx, WIN_X + WIN_W - 41
    mov     edx, WIN_Y + 3
    mov     ebx, 18
    mov     esi, WIN_TH - 5
    mov     eax, 0x008080A0
    call    fb_fill_rect
    mov     ecx, WIN_X + WIN_W - 37
    mov     edx, WIN_Y + 7
    mov     esi, s_btn_max
    mov     ebx, C_WHITE
    mov     edi, 0x008080A0
    call    fb_puts

    ; Barre de menus
    mov     ecx, WIN_X + 2
    mov     edx, WIN_Y + WIN_TH
    mov     ebx, WIN_W - 4
    mov     esi, WIN_MH
    mov     eax, C_WINCLI
    call    fb_fill_rect
    mov     ecx, WIN_X + 8
    mov     edx, WIN_Y + WIN_TH + 4
    mov     esi, s_menubar
    mov     ebx, C_BLACK
    mov     edi, C_WINCLI
    call    fb_puts

    ; Zone terminal (fond noir)
    mov     ecx, WIN_TX
    mov     edx, WIN_TY
    mov     ebx, WIN_TW
    mov     esi, WIN_THGT
    mov     eax, C_TERMBG
    call    fb_fill_rect

    ; Texte de bienvenue
    mov     ecx, WIN_TX + 4
    mov     edx, WIN_TY + 4
    mov     esi, s_vesa_welcome
    mov     ebx, C_TERMGRN
    mov     edi, C_TERMBG
    call    fb_puts

    mov     ecx, WIN_TX + 4
    mov     edx, WIN_TY + 14
    mov     esi, s_vesa_hint
    mov     ebx, C_TERMFG
    mov     edi, C_TERMBG
    call    fb_puts

    ; Curseur shell commence apres le texte de bienvenue
    mov     dword [vesa_cur_x], WIN_TX + 4
    mov     dword [vesa_cur_y], WIN_TY + 30

    popad
    ret

; vesa_putc: sortie texte sur le terminal VESA (remplace vga_putc)
; Memes conventions: AL = caractere, gere \n et backspace
vesa_putc:
    push    eax
    push    ecx
    push    edx
    push    ebx
    push    esi
    push    edi

    cmp     al, 10             ; newline
    je      .nl
    cmp     al, 8              ; backspace
    je      .bs

    ; Caractere normal: dessine et avance
    mov     ecx, [vesa_cur_x]
    mov     edx, [vesa_cur_y]
    movzx   ebx, byte [vga_attr]
    ; Convertit attr VGA 4-bit en couleur 24-bit
    call    vga_attr_to_rgb    ; eax = fg, esi = bg
    call    fb_putchar
    add     dword [vesa_cur_x], 8
    mov     eax, [vesa_cur_x]
    cmp     eax, WIN_TX + WIN_TW - 8
    jle     .done
    ; Wrap de ligne
    mov     dword [vesa_cur_x], WIN_TX + 4
    add     dword [vesa_cur_y], 8
    jmp     .scroll_check

.nl:
    mov     dword [vesa_cur_x], WIN_TX + 4
    add     dword [vesa_cur_y], 8
    jmp     .scroll_check

.bs:
    cmp     dword [vesa_cur_x], WIN_TX + 12
    jle     .done
    sub     dword [vesa_cur_x], 8
    ; Efface le caractere precedent
    mov     ecx, [vesa_cur_x]
    mov     edx, [vesa_cur_y]
    mov     ebx, 8
    mov     esi, 8
    mov     eax, C_TERMBG
    call    fb_fill_rect
    jmp     .done

.scroll_check:
    mov     eax, [vesa_cur_y]
    cmp     eax, WIN_TY + WIN_THGT - 10
    jle     .done
    ; Reset en haut du terminal
    mov     dword [vesa_cur_y], WIN_TY + 30
    mov     ecx, WIN_TX
    mov     edx, WIN_TY + 30
    mov     ebx, WIN_TW
    mov     esi, WIN_THGT - 30
    mov     eax, C_TERMBG
    call    fb_fill_rect

.done:
    pop     edi
    pop     esi
    pop     ebx
    pop     edx
    pop     ecx
    pop     eax
    ret

; vga_attr_to_rgb: convertit [vga_attr] (4-bit VGA) en eax=fg, esi=bg
; Palette VGA 16 couleurs simplifiee
vga_attr_to_rgb:
    push    ebx
    movzx   eax, byte [vga_attr]
    mov     ebx, eax
    and     eax, 0x0F          ; fg index
    and     ebx, 0xF0
    shr     ebx, 4             ; bg index
    ; Lookup fg
    lea     eax, [vga_palette + eax*4]
    mov     eax, [eax]
    ; Lookup bg
    lea     esi, [vga_palette + ebx*4]
    mov     esi, [esi]
    pop     ebx
    ret

; ===== DONNEES =====

; Variables VGA
vga_col  dd 0
vga_row  dd 2
vga_attr db A_OUT

; Timer
ticks    dd 0

; Position sauvegardee pour refresh
saved_col dd 1
saved_row dd 2

; Clavier
kbd_buf  times KBUF_SZ db 0
kbd_head dd 0
kbd_tail dd 0
kbd_sft  db 0
kbd_cap  db 0

; Command buffer
cmd_buf  times CMD_LEN db 0

; IDT
align 4
idt_table times 256*8 db 0
idt_ptr:
    dw 256*8-1
    dd idt_table

; Strings interface
s_mtitle  db ' MyOS v2.0 ', 0
s_mfich   db ' Fichier ', 0
s_moutils db ' Outils ', 0
s_maide   db ' Aide ', 0
s_uplab   db 'Up:', 0
s_status  db ' help ls ps top df dmesg ifconfig ping fortune cowsay cal about reboot', 0
s_wintitle db ' Terminal - root@myos:/ ', 0
s_welcome  db 'MyOS v2.0 - Kernel x86 32bit ASM pur', 10, 0
s_hint     db 'Tape "help" pour les commandes.', 10, 10, 0
s_puser    db 'root@myos', 0
s_ppath    db ':/ ', 0
s_pend     db '# ', 0
s_unk      db 'Commande inconnue: ', 0
s_exc      db '*** EXCEPTION CPU ***', 0
s_upt      db 'Uptime: ', 0
s_sec      db ' secondes', 10, 0
s_colok    db 'Couleur changee.', 10, 0
s_rbt      db 'Reboot...', 10, 0
s_uname    db 'MyOS 2.0 x86 32-bit - 0 ligne de C - ASM pur', 10, 0
s_help:
    db '--- MyOS v2.0 Commandes Kernel ---', 10
    db 'Fichiers:', 10
    db '  ls ll dir pwd  lister / repertoire', 10
    db '  cat [fichier]  afficher fichier', 10
    db '  mkdir touch rm cp mv stat file', 10
    db '  find grep chmod chown', 10
    db 'Systeme:', 10
    db '  uname arch whoami id hostname date', 10
    db '  time uptime ps top kill free df du', 10
    db '  mem mount dmesg lscpu lspci lsblk', 10
    db '  lsusb lsmod env sync history', 10
    db 'Reseau:', 10
    db '  ifconfig net netstat ping', 10
    db 'Affichage:', 10
    db '  echo color clear cls gfx banner', 10
    db '  fortune cowsay matrix sl creeper cal', 10
    db 'Shell:', 10
    db '  man which sudo su bash exit about', 10
    db '  sleep reboot shutdown poweroff', 10, 0

sc_help   db 'help', 0
sc_clear  db 'clear', 0
sc_echo   db 'echo ', 0
sc_color  db 'color ', 0
sc_up     db 'uptime', 0
sc_uname  db 'uname', 0
sc_gfx    db 'gfx', 0
sc_reboot db 'reboot', 0

; --- Commandes supplementaires ---
sc_whoami    db 'whoami', 0
sc_id        db 'id', 0
sc_hostname  db 'hostname', 0
sc_arch      db 'arch', 0
sc_date      db 'date', 0
sc_pwd       db 'pwd', 0
sc_ls        db 'ls', 0
sc_ls_pfx    db 'ls ', 0
sc_ll        db 'll', 0
sc_dir       db 'dir', 0
sc_cat_cpu   db 'cat /proc/cpuinfo', 0
sc_cat_mem   db 'cat /proc/meminfo', 0
sc_cat_host  db 'cat /etc/hostname', 0
sc_cat_osr   db 'cat /etc/os-release', 0
sc_cat_ver   db 'cat /proc/version', 0
sc_cat_pfx   db 'cat ', 0
sc_ps        db 'ps', 0
sc_top       db 'top', 0
sc_free      db 'free', 0
sc_df        db 'df', 0
sc_dmesg     db 'dmesg', 0
sc_lscpu     db 'lscpu', 0
sc_lspci     db 'lspci', 0
sc_lsblk     db 'lsblk', 0
sc_lsusb     db 'lsusb', 0
sc_lsmod     db 'lsmod', 0
sc_mem       db 'mem', 0
sc_mount     db 'mount', 0
sc_env       db 'env', 0
sc_ifc       db 'ifconfig', 0
sc_net       db 'net', 0
sc_netstat   db 'netstat', 0
sc_ping      db 'ping', 0
sc_ping_pfx  db 'ping ', 0
sc_about     db 'about', 0
sc_fortune   db 'fortune', 0
sc_cowsay    db 'cowsay', 0
sc_cowsay_pfx db 'cowsay ', 0
sc_matrix    db 'matrix', 0
sc_sl        db 'sl', 0
sc_creeper   db 'creeper', 0
sc_true      db 'true', 0
sc_false     db 'false', 0
sc_exit      db 'exit', 0
sc_logout    db 'logout', 0
sc_shutdown  db 'shutdown', 0
sc_poweroff  db 'poweroff', 0
sc_halt_cmd  db 'halt', 0
sc_sudo      db 'sudo', 0
sc_sudo_pfx  db 'sudo ', 0
sc_su        db 'su', 0
sc_bash      db 'bash', 0
sc_sh        db 'sh', 0
sc_zsh       db 'zsh', 0
sc_sync      db 'sync', 0
sc_history   db 'history', 0
sc_cal       db 'cal', 0
sc_cls       db 'cls', 0
sc_time      db 'time', 0
sc_sleep     db 'sleep', 0
sc_sleep_pfx db 'sleep ', 0
sc_banner    db 'banner', 0
sc_banner_pfx db 'banner ', 0
sc_man_pfx   db 'man ', 0
sc_which_pfx db 'which ', 0
sc_kill_pfx  db 'kill ', 0
sc_mkdir_pfx db 'mkdir ', 0
sc_touch_pfx db 'touch ', 0
sc_rm_pfx    db 'rm ', 0
sc_cp_pfx    db 'cp ', 0
sc_mv_pfx    db 'mv ', 0
sc_chmod_pfx db 'chmod ', 0
sc_chown_pfx db 'chown ', 0
sc_grep_pfx  db 'grep ', 0
sc_grep      db 'grep', 0
sc_find_pfx  db 'find ', 0
sc_find      db 'find', 0
sc_du_pfx    db 'du ', 0
sc_du        db 'du', 0
sc_stat_pfx  db 'stat ', 0
sc_file_pfx  db 'file ', 0
sc_strace_pfx db 'strace', 0
sc_gdb_pfx   db 'gdb', 0
sc_make_pfx  db 'make', 0
sc_gcc_pfx   db 'gcc', 0
sc_curl_pfx  db 'curl', 0
sc_wget_pfx  db 'wget', 0
sc_ssh_pfx   db 'ssh', 0
sc_echo_bare db 'echo', 0
sc_head_pfx  db 'head ', 0
sc_tail_pfx  db 'tail ', 0
sc_wc_pfx    db 'wc ', 0
sc_sort_pfx  db 'sort', 0
sc_sed_pfx   db 'sed ', 0
sc_awk_pfx   db 'awk ', 0

; Reponses des nouvelles commandes
s_whoami     db 'root', 10, 0
s_id         db 'uid=0(root) gid=0(root) groups=0(root)', 10, 0
s_hostname_out db 'myos.epitech.eu', 10, 0
s_arch       db 'i386', 10, 0
s_date       db 'Mer 21 Mai 2026 00:00:00 CET', 10, 0
s_pwd        db '/', 10, 0
s_ls:
    db 'drwxr-xr-x  bin/', 10
    db 'drwxr-xr-x  boot/', 10
    db 'drwxr-xr-x  dev/', 10
    db 'drwxr-xr-x  etc/', 10
    db 'drwxr-xr-x  home/', 10
    db 'drwxr-xr-x  lib/', 10
    db 'drwxr-xr-x  proc/', 10
    db 'drwxr-xr-x  sys/', 10
    db 'drwxr-xr-x  tmp/', 10
    db 'drwxr-xr-x  usr/', 10
    db 'drwxr-xr-x  var/', 10, 0
s_cat_cpu:
    db 'processor  : 0', 10
    db 'vendor_id  : GenuineIntel', 10
    db 'model name : i386 MyOS CPU @ 1GHz', 10
    db 'cpu MHz    : 1000.000', 10
    db 'cache size : 256 KB', 10, 0
s_cat_mem:
    db 'MemTotal:    131072 kB', 10
    db 'MemFree:      98304 kB', 10
    db 'Buffers:       4096 kB', 10
    db 'Cached:        8192 kB', 10, 0
s_cat_osr:
    db 'NAME="MyOS"', 10
    db 'VERSION="2.0 Epitech"', 10
    db 'ID=myos', 10
    db 'HOME_URL=https://epitech.eu', 10, 0
s_cat_ver:
    db 'MyOS version 2.0 (gcc 12.2.0) #1 SMP 2026', 10, 0
s_cat_gen:
    db '(fichier binaire ou non accessible)', 10, 0
s_ps:
    db '  PID TTY  STAT CMD', 10
    db '    1 ?    Ss   init', 10
    db '    2 ?    S    kthreadd', 10
    db '    3 ?    S    kmain', 10
    db '   10 tty0 R    sh', 10
    db '   11 tty0 R+   ps', 10, 0
s_top:
    db 'Tasks:  5 total, 1 running', 10
    db 'CPU:  0.1%us  0.0%sy', 10
    db 'Mem: 131072k total, 32768k used, 98304k free', 10, 10
    db '  PID  %CPU  %MEM  CMD', 10
    db '    1   0.0   0.0  init', 10
    db '    3   0.0   0.5  kmain', 10
    db '   10   0.1   0.0  sh', 10, 0
s_free_out:
    db '              total    used    free', 10
    db 'Mem:         131072   32768   98304', 10
    db 'Swap:             0       0       0', 10, 0
s_df:
    db 'Filesystem   Size  Used Avail Use%', 10
    db '/dev/hda     1.4M  890K  510K  64%', 10
    db 'tmpfs         64M    0K   64M   0%', 10, 0
s_dmesg:
    db '[    0.000] MyOS kernel started', 10
    db '[    0.001] Protected mode active (GDT ok)', 10
    db '[    0.002] IDT installed, IRQs configured', 10
    db '[    0.003] VESA VBE 640x480 24bpp init', 10
    db '[    0.004] PS/2 keyboard driver loaded', 10
    db '[    0.005] PS/2 mouse driver loaded', 10
    db '[    0.006] PIC remapped: IRQ0-7 -> 0x20', 10
    db '[    0.007] PIT channel 0 @ 100Hz', 10
    db '[    0.008] PCI bus scan: 5 devices found', 10
    db '[    0.009] RTL8139 Ethernet at 00:03.0', 10
    db '[    0.010] MineGRUB handoff complete', 10, 0
s_lscpu:
    db 'Architecture:  i386', 10
    db 'CPU op-mode:   32-bit', 10
    db 'CPU(s):        1', 10
    db 'Vendor ID:     GenuineIntel', 10
    db 'Model name:    i386 compatible @ 1GHz', 10
    db 'CPU MHz:       1000.000', 10
    db 'L1d cache:     16K', 10
    db 'L2 cache:      256K', 10
    db 'Flags:         fpu pse pae mce mtrr pge', 10, 0
s_lspci:
    db '00:00.0 Host bridge: Intel i440FX', 10
    db '00:01.0 ISA bridge : Intel PIIX3', 10
    db '00:02.0 VGA compat : Standard VESA', 10
    db '00:03.0 Ethernet   : Realtek RTL8139', 10
    db '00:04.0 Audio      : Intel AC97', 10, 0
s_lsblk:
    db 'NAME  SIZE TYPE MOUNTPOINT', 10
    db 'hda   1.4M disk /', 10
    db 'fd0   1.4M disk', 10, 0
s_lsusb:
    db 'Bus 001 Device 001: ID 8086:7020 UHCI Root Hub', 10, 0
s_lsmod:
    db 'Module        Size  Used by', 10
    db 'rtl8139       8192  0', 10
    db 'ps2kbd        4096  0', 10
    db 'ps2mouse      4096  0', 10
    db 'vesa          8192  0', 10
    db 'pit_timer     4096  0', 10, 0
s_mem:
    db 'Carte memoire:', 10
    db '  0x000000-0x0004FF  IVT + BDA (mode reel)', 10
    db '  0x000500-0x0007FF  Boot info (MineGRUB)', 10
    db '  0x001000-0x00FFFF  Stack / zone libre', 10
    db '  0x010000-0x02FFFF  Kernel ASM (code+data)', 10
    db '  0x300000-0x3FFFFF  Back-buffer VESA', 10
    db '  0x400000-0x4FFFFF  Heap noyau', 10
    db '  Total RAM detecte: 128 MB', 10, 0
s_mount:
    db '/dev/hda   on  /     type ext2  (rw,relatime)', 10
    db 'none       on  /proc type proc  (rw)', 10
    db 'tmpfs      on  /tmp  type tmpfs (rw)', 10, 0
s_env:
    db 'PATH=/bin:/usr/bin:/usr/local/bin', 10
    db 'HOME=/root', 10
    db 'USER=root', 10
    db 'SHELL=/bin/sh', 10
    db 'TERM=myos-vt', 10
    db 'LANG=fr_FR.UTF-8', 10
    db 'HOSTNAME=myos.epitech.eu', 10
    db 'OSTYPE=myos', 10, 0
s_ifconfig:
    db 'lo    Link encap:Local Loopback', 10
    db '      inet addr:127.0.0.1  Mask:255.0.0.0', 10
    db '      UP LOOPBACK RUNNING  MTU:65536', 10, 10
    db 'eth0  Link encap:Ethernet  HWaddr 52:54:00:12:34:56', 10
    db '      inet addr:10.0.2.15  Mask:255.255.255.0', 10
    db '      UP BROADCAST RUNNING  MTU:1500', 10, 0
s_netstat:
    db 'Proto  LocalAddr          ForeignAddr    State', 10
    db 'tcp    127.0.0.1:0        0.0.0.0:0      LISTEN', 10
    db 'udp    10.0.2.15:68       0.0.0.0:0      -', 10, 0
s_ping:
    db 'PING 127.0.0.1 56(84) bytes of data.', 10
    db '64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.42ms', 10
    db '64 bytes from 127.0.0.1: icmp_seq=2 ttl=64 time=0.38ms', 10
    db '64 bytes from 127.0.0.1: icmp_seq=3 ttl=64 time=0.41ms', 10
    db '3 paquets transmis, 0% perte', 10, 0
s_about:
    db '================================', 10
    db '  MyOS v2.0 - Epitech Technology', 10
    db '  Architecture  : x86 32-bit', 10
    db '  Bootloader    : MineGRUB (ASM)', 10
    db '  Kernel        : ASM pur (NASM)', 10
    db '  Video         : VESA 640x480 24bpp', 10
    db '  Auteur        : Epitech Barcelona', 10
    db '================================', 10, 0
s_cowsay:
    db ' --------------------------', 10
    db '< Mooooo! MyOS is alive!  >', 10
    db ' --------------------------', 10
    db '         \   ^__^', 10
    db '          \  (oo)\_______', 10
    db '             (__)\       )\/\', 10
    db '                 ||----w |', 10
    db '                 ||     ||', 10, 0
s_matrix:
    db 'Wake up, Neo...', 10
    db 'The Matrix has you.', 10
    db 'Follow the white rabbit.', 10
    db 'Knock, knock, Neo.', 10, 0
s_sl:
    db '        ====        ________', 10
    db '    _D _|  |_______/        \__', 10
    db '   |(_)---  |   H\________/ |', 10
    db '   /     |  |   H  |  |  |  |', 10
    db '  | | | |    \_____/ |__|__|', 10, 0
s_creeper:
    db '  Creeper, Aw Man...', 10
    db '  +---------+', 10
    db '  |  ##  ## |', 10
    db '  |  ##  ## |', 10
    db '  |   ####  |', 10
    db '  |  ######  |', 10
    db '  | ##  ## # |', 10
    db '  +---------+', 10, 0
s_false_out  db 'false: exit status 1', 10, 0
s_exit       db '(pas de session parente a quitter)', 10, 0
s_sudo       db '[sudo] MyOS: root@myos - acces accorde', 10, 0
s_shell      db 'MyOS Shell v2.0 (deja en cours d execution)', 10, 0
s_history    db '(historique: non disponible en kernel ASM pur)', 10, 0
s_cal:
    db '       Mai 2026', 10
    db 'Lu Ma Me Je Ve Sa Di', 10
    db '             1  2  3', 10
    db ' 4  5  6  7  8  9 10', 10
    db '11 12 13 14 15 16 17', 10
    db '18 19 20 *21 22 23 24', 10
    db '25 26 27 28 29 30 31', 10, 0
s_time       db 'uptime (ticks): ', 0
s_banner:
    db '  ###   ###  #  #  ###', 10
    db '  #  #  #  # #  # #   ', 10
    db '  ###   ###  #  #  ## ', 10
    db '  # #   #    #  #    #', 10
    db '  #  #  #     ##  ###  -- MyOS', 10, 0
s_man:
    db 'MAN(1) MyOS Reference Manual', 10
    db 'Commandes: help, ls, ps, top, free, df,', 10
    db '  dmesg, lscpu, lspci, ifconfig, ping,', 10
    db '  uname, date, fortune, cowsay, reboot', 10
    db 'Tapez "help" pour la liste complete.', 10, 0
s_which      db '/bin/', 10, 0
s_kill       db 'kill: processus introuvable', 10, 0
s_mkdir      db 'mkdir: repertoire cree (VFS virtuel)', 10, 0
s_touch      db 'touch: fichier mis a jour (VFS virtuel)', 10, 0
s_rm         db 'rm: fichier supprime (VFS virtuel)', 10, 0
s_cp         db 'cp: copie effectuee (VFS virtuel)', 10, 0
s_mv         db 'mv: fichier deplace (VFS virtuel)', 10, 0
s_grep       db 'grep: aucun resultat correspondant', 10, 0
s_find_out:
    db '/', 10
    db '/bin', 10
    db '/etc', 10
    db '/proc', 10
    db '/home/root', 10
    db '/var', 10, 0
s_du         db '0       .', 10, 0
s_stat       db 'Fichier: ?  Taille: 0  Mode: 644  Uid: 0', 10, 0
s_file       db 'data: ASCII text', 10, 0
s_nosupport  db 'Erreur: non supporte en baremetal (strace/gdb)', 10, 0
s_nosupport2 db 'Erreur: pas de compilateur runtime (make/gcc)', 10, 0
s_nosupport3 db 'Erreur: reseau HTTP non supporte ici (curl/wget)', 10, 0
s_nosupport4 db 'Erreur: SSH/FTP non supporte en baremetal', 10, 0
s_wc         db '  0  0  0 (stdin)', 10, 0
s_stdin      db '(stdin: entree standard non supportee)', 10, 0

; Table fortune (8 citations)
align 4
fort_table:
    dd s_fort0, s_fort1, s_fort2, s_fort3
    dd s_fort4, s_fort5, s_fort6, s_fort7
fortune_idx dd 0

s_fort0 db 'Creeper, Aw Man...', 10, 0
s_fort1 db 'make: *** [all] Error 1 -- classique.', 10, 0
s_fort2 db "There's no place like 127.0.0.1", 10, 0
s_fort3 db 'sudo make me a sandwich.', 10, 0
s_fort4 db 'The answer is 42.', 10, 0
s_fort5 db 'rm -rf /* -- Ouf, machine virtuelle!', 10, 0
s_fort6 db '42h sans dormir = hacker mode: ON', 10, 0
s_fort7 db ':(){:|:&};:  <- fork bomb (blague ASM)', 10, 0

; ===== VESA variables =====
vesa_active  db 0
fb_addr      dd 0
vesa_font    dd 0
vesa_cur_x   dd 0
vesa_cur_y   dd 0

; Variables internes fb_fill_rect / fb_putchar
frc_color    dd 0
fc_fg        dd 0
fc_bg        dd 0
fc_x         dd 0
fc_y         dd 0
fc_row       dd 0
fc_col       dd 0

; Palette VGA 16 couleurs en 0x00RRGGBB
vga_palette:
    dd 0x00000000   ; 0  noir
    dd 0x000000AA   ; 1  bleu sombre
    dd 0x0000AA00   ; 2  vert sombre
    dd 0x0000AAAA   ; 3  cyan sombre
    dd 0x00AA0000   ; 4  rouge sombre
    dd 0x00AA00AA   ; 5  magenta sombre
    dd 0x00AA5500   ; 6  marron
    dd 0x00AAAAAA   ; 7  gris clair
    dd 0x00555555   ; 8  gris fonce
    dd 0x005555FF   ; 9  bleu vif
    dd 0x0055FF55   ; A  vert vif
    dd 0x0055FFFF   ; B  cyan vif
    dd 0x00FF5555   ; C  rouge vif
    dd 0x00FF55FF   ; D  magenta vif
    dd 0x00FFFF55   ; E  jaune
    dd 0x00FFFFFF   ; F  blanc

; Strings VESA desktop
s_start        db 'Start', 0
s_clock        db '00:00', 0
s_vesa_title   db 'MineOS Shell  [MyOS v2.0 - Pure ASM]', 0
s_menubar      db 'Fichier   Edition   Affichage   Aide', 0
s_btn_x        db 'x', 0
s_btn_max      db '+', 0
s_vesa_welcome db 'MineOS v2.0 - Mode graphique VESA 640x480x24bpp', 0
s_vesa_hint    db 'Tape "help" pour les commandes disponibles.', 0
