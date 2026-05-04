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
