# MyOS — OS en assembleur x86 pur

Zéro C. Zéro libc. Chaque instruction choisie à la main.

## Structure

```
myos_asm/
├── stage1/boot.asm     512 octets exactement (MBR)
├── stage2/stage2.asm   GDT, A20, passage en mode protégé 32-bit
├── kernel/kernel.asm   Kernel complet : IDT, PIC, PIT, clavier, VGA, shell
└── Makefile            make && make run
```

## Disposition mémoire

```
0x00007C00  Stage1 (MBR, 512 octets) — chargé par le BIOS
0x00008000  Stage2 (16KB max)        — chargé par stage1
0x00010000  Kernel (32KB max)        — chargé par stage2
0x000B8000  Mémoire vidéo VGA texte  — accès direct par le kernel
0x00200000  Pile kernel              — 2MB
```

## Disposition de l'image floppy

```
Secteur 0      : Stage1 MBR (512 octets)
Secteurs 1-32  : Stage2    (16KB)
Secteurs 33-96 : Kernel    (32KB)
```

## Séquence de boot

```
BIOS
  └─► charge secteur 0 à 0x7C00 et l'exécute
        └─► Stage1 (mode réel 16-bit)
              ├─ Active A20
              ├─ Charge stage2 depuis disque à 0x8000
              └─► Stage2 (toujours mode réel)
                    ├─ Construit la GDT en mémoire
                    ├─ Active le bit PE dans CR0
                    ├─ Far jump → mode protégé 32-bit
                    ├─ Charge le kernel à 0x10000
                    └─► Kernel (mode protégé 32-bit)
                          ├─ IDT (256 entrées)
                          ├─ PIC 8259 reconfiguré (IRQ 32-47)
                          ├─ PIT à 100 Hz (IRQ0)
                          ├─ Driver clavier PS/2 (IRQ1)
                          ├─ Driver VGA direct
                          └─► Shell interactif
```

## Outils requis (rien d'autre)

```bash
# Ubuntu/Debian/WSL
sudo apt-get install nasm qemu-system-x86

# macOS
brew install nasm qemu
```

## Compiler et lancer

```bash
make          # Assemble les 3 binaires + crée l'image floppy
make run      # Lance dans QEMU
make debug    # Lance avec GDB sur port :1234
make disasm   # Désassemble le kernel (nécessite ndisasm)
```

## Commandes du shell

| Commande | Effet |
|----------|-------|
| `help` | Liste les commandes |
| `clear` | Efface l'écran |
| `echo [texte]` | Affiche du texte |
| `hello` | Message de bienvenue |
| `regs` | Affiche les registres |
| `uptime` | Secondes depuis le boot |
| `color [0-15]` | Change la couleur du texte |
| `reboot` | Redémarre |

## Ajouter une commande

Dans `kernel/kernel.asm`, dans `cmd_execute` :

```asm
; Détection
push esi
mov edi, str_macommande
call str_eq
pop esi
test eax, eax
jnz .do_macommande

; Handler
.do_macommande:
    call cmd_macommande
    jmp .done

; Implémentation
cmd_macommande:
    mov esi, mon_message
    call vga_print
    ret

; Données
str_macommande db 'macommande', 0
mon_message    db 'Hello!', 0x0A, 0
```

Juste `nasm` qui traduit de l'assembleur en opcodes x86 bruts.
