[BITS 32]

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    cld
    ; Zero BSS avant d'entrer en C
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    call kmain
.halt:
    cli
    hlt
    jmp .halt

; =============================================================
; ISR IRQ0 (PIT) : incremente le compteur de ticks a 0x046C
; (meme adresse que le compteur BIOS => BIOS_TICKS redevient reel)
; =============================================================
global irq0_stub
irq0_stub:
    push eax
    inc dword [0x046C]
    mov al, 0x20
    out 0x20, al
    pop eax
    iretd

; ISR par defaut pour les autres IRQ (EOI aux deux PIC, ignore)
global irq_ignore_stub
irq_ignore_stub:
    push eax
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    pop eax
    iretd
