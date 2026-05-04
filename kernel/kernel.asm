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
s_status  db ' help  clear  echo  color  uptime  uname  gfx  reboot', 0
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
    db '--- Commandes ---', 10
    db '  help           cette aide', 10
    db '  clear          redessine le bureau', 10
    db '  echo [texte]   affiche du texte', 10
    db '  color [0-15]   couleur texte', 10
    db '  uptime         temps depuis boot', 10
    db '  uname          info systeme', 10
    db '  gfx            demo 16 couleurs VGA', 10
    db '  reboot         redemarrage', 10, 0

sc_help   db 'help', 0
sc_clear  db 'clear', 0
sc_echo   db 'echo ', 0
sc_color  db 'color ', 0
sc_up     db 'uptime', 0
sc_uname  db 'uname', 0
sc_gfx    db 'gfx', 0
sc_reboot db 'reboot', 0

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
