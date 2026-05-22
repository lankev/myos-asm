from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.util import Inches, Pt
import pptx.oxml.ns as nsmap
from lxml import etree

prs = Presentation()
prs.slide_width  = Inches(13.33)
prs.slide_height = Inches(7.5)

# ── Palette ──────────────────────────────────────────────────────────────────
BG_DARK   = RGBColor(0x0D, 0x1B, 0x2A)
BG_MID    = RGBColor(0x1B, 0x31, 0x4F)
ACCENT    = RGBColor(0x00, 0xC8, 0xFF)
ACCENT2   = RGBColor(0x7C, 0x4D, 0xFF)
GREEN     = RGBColor(0x39, 0xD3, 0x53)
ORANGE    = RGBColor(0xFF, 0x8C, 0x00)
WHITE     = RGBColor(0xFF, 0xFF, 0xFF)
LIGHT     = RGBColor(0xC8, 0xD8, 0xE8)
YELLOW    = RGBColor(0xFF, 0xE0, 0x40)
RED_SOFT  = RGBColor(0xFF, 0x4E, 0x4E)

BLANK_LAYOUT = prs.slide_layouts[6]

# ── Low-level helpers ─────────────────────────────────────────────────────────

def solid_fill(shape, rgb: RGBColor):
    shape.fill.solid()
    shape.fill.fore_color.rgb = rgb

def add_rect(slide, l, t, w, h, rgb):
    shp = slide.shapes.add_shape(1, Inches(l), Inches(t), Inches(w), Inches(h))
    solid_fill(shp, rgb)
    shp.line.fill.background()
    return shp

def add_text(slide, text, l, t, w, h,
             font_size=18, bold=False, color=WHITE,
             align=PP_ALIGN.LEFT, italic=False, wrap=True):
    txb = slide.shapes.add_textbox(Inches(l), Inches(t), Inches(w), Inches(h))
    txb.word_wrap = wrap
    tf = txb.text_frame
    tf.word_wrap = wrap
    p = tf.paragraphs[0]
    p.alignment = align
    run = p.add_run()
    run.text = text
    run.font.size = Pt(font_size)
    run.font.bold = bold
    run.font.italic = italic
    run.font.color.rgb = color
    return txb

def slide_bg(slide, rgb):
    bg = slide.background
    fill = bg.fill
    fill.solid()
    fill.fore_color.rgb = rgb

def add_bullet_block(slide, items, l, t, w, h,
                     font_size=16, color=LIGHT, bullet="▶  "):
    txb = slide.shapes.add_textbox(Inches(l), Inches(t), Inches(w), Inches(h))
    txb.word_wrap = True
    tf = txb.text_frame
    tf.word_wrap = True
    first = True
    for item in items:
        p = tf.paragraphs[0] if first else tf.add_paragraph()
        first = False
        run = p.add_run()
        run.text = bullet + item
        run.font.size = Pt(font_size)
        run.font.color.rgb = color
    return txb

def add_code_block(slide, lines, l, t, w, h, font_size=11):
    bg = add_rect(slide, l, t, w, h, RGBColor(0x05, 0x10, 0x1A))
    txb = slide.shapes.add_textbox(Inches(l+0.1), Inches(t+0.08),
                                   Inches(w-0.2), Inches(h-0.16))
    txb.word_wrap = False
    tf = txb.text_frame
    tf.word_wrap = False
    first = True
    for line in lines:
        p = tf.paragraphs[0] if first else tf.add_paragraph()
        first = False
        run = p.add_run()
        run.text = line
        run.font.size = Pt(font_size)
        run.font.color.rgb = GREEN
        run.font.name = "Consolas"
    return txb

def header_bar(slide, title_text, subtitle=""):
    add_rect(slide, 0, 0, 13.33, 1.1, BG_MID)
    add_rect(slide, 0, 1.05, 13.33, 0.07, ACCENT)
    add_text(slide, title_text, 0.3, 0.08, 10, 0.65,
             font_size=30, bold=True, color=WHITE)
    if subtitle:
        add_text(slide, subtitle, 0.3, 0.68, 10, 0.35,
                 font_size=14, color=ACCENT, italic=True)

def footer_bar(slide, text="MyOS — Epitech Project  |  2026"):
    add_rect(slide, 0, 7.15, 13.33, 0.35, BG_MID)
    add_text(slide, text, 0.2, 7.17, 13, 0.3,
             font_size=11, color=RGBColor(0x80,0xA0,0xC0), align=PP_ALIGN.LEFT)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 1 — Title
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
add_rect(s, 0, 0, 13.33, 2.2, BG_MID)
add_rect(s, 0, 2.15, 13.33, 0.08, ACCENT)
add_text(s, "MyOS", 1, 0.25, 11, 1.5,
         font_size=80, bold=True, color=ACCENT, align=PP_ALIGN.CENTER)
add_text(s, "An operating system built entirely from scratch", 1, 1.65, 11, 0.7,
         font_size=22, bold=False, color=WHITE, align=PP_ALIGN.CENTER, italic=True)
add_rect(s, 2, 2.6, 9.33, 0.05, ACCENT2)
add_text(s, "x86 32-bit  ·  NASM Assembly + Freestanding C  ·  VESA 800×600  ·  180+ commands",
         0.5, 2.8, 12.3, 0.6,
         font_size=16, color=LIGHT, align=PP_ALIGN.CENTER)
techs = [("ASM", ACCENT2), ("C", GREEN), ("VESA", ORANGE), ("RTL8139", YELLOW), ("PS/2", RED_SOFT)]
x = 2.2
for lab, col in techs:
    add_rect(s, x, 3.65, 1.5, 0.5, col)
    add_text(s, lab, x, 3.65, 1.5, 0.5,
             font_size=16, bold=True, color=WHITE, align=PP_ALIGN.CENTER)
    x += 1.8
add_rect(s, 0, 6.3, 13.33, 1.2, RGBColor(0x08, 0x14, 0x22))
add_text(s, "Epitech Project — Low-level Systems & Innovation",
         0.5, 6.45, 12, 0.45, font_size=15, color=LIGHT, align=PP_ALIGN.CENTER)
add_text(s, "2025 – 2026", 0.5, 6.88, 12, 0.38,
         font_size=13, color=ACCENT, align=PP_ALIGN.CENTER)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 2 — Context & Goals
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Context & Goals", "Why build an OS from scratch?")
footer_bar(s)

add_rect(s, 0.3, 1.3, 5.9, 5.5, BG_MID)
add_rect(s, 0.3, 1.3, 5.9, 0.05, ACCENT)
add_text(s, "The Challenge", 0.5, 1.35, 5.5, 0.55, font_size=20, bold=True, color=ACCENT)
add_bullet_block(s, [
    "Build a complete OS with zero external libraries",
    "Everything from scratch: bootloader, kernel, libc, graphics",
    "Hard constraint: 0 third-party dependencies",
    "Target: x86 32-bit, bare-metal (no host OS)",
    "Graphical interface VESA 800x600 24bpp",
    "Interactive terminal with 180+ Linux-like commands",
    "Games: Snake, R-Type, Pong — all in the OS",
], 0.5, 1.95, 5.5, 4.6, font_size=15)

add_rect(s, 6.9, 1.3, 6.1, 5.5, BG_MID)
add_rect(s, 6.9, 1.3, 6.1, 0.05, ACCENT2)
add_text(s, "Tech Stack", 6.9, 1.35, 5.7, 0.55,
         font_size=20, bold=True, color=ACCENT2)
add_bullet_block(s, [
    "NASM — assembler for stage1 / stage2",
    "GCC -m32 -ffreestanding — kernel C without libc",
    "LD — custom linker script (entry at 0x10000)",
    "QEMU — emulation & debugging",
    "VirtualBox / Bare-metal — physical testing",
    "Make — complete build system",
    "Git — version control",
], 6.9, 1.95, 5.8, 4.6, font_size=15)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 3 — Overall Architecture
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Overall Architecture", "From BIOS to kernel in 3 stages")
footer_bar(s)

boxes = [
    ("Stage 1\nMBR\n512 bytes",   ACCENT,   1.0, 1.5),
    ("Stage 2\nMineGRUB\n8 KB",   ACCENT2,  4.0, 1.5),
    ("Kernel C\nkmain.c\n~140 KB", GREEN,   7.0, 1.5),
    ("Applications\nSFCML + Shell\n180+ cmds", ORANGE, 10.0, 1.5),
]
for label, col, x, y in boxes:
    add_rect(s, x, y, 2.5, 1.6, col)
    add_text(s, label, x, y, 2.5, 1.6,
             font_size=15, bold=True, color=WHITE, align=PP_ALIGN.CENTER)
    if x < 10.0:
        add_text(s, "→", x+2.5, y+0.55, 0.5, 0.5,
                 font_size=28, bold=True, color=WHITE, align=PP_ALIGN.CENTER)

add_text(s, "Disk layout (1.44 MB)", 0.3, 3.4, 12.5, 0.45,
         font_size=18, bold=True, color=ACCENT)
sectors = [
    ("Sector 0\nMBR Stage1\n512 B",      BG_MID,                         0.3, 3.9, 1.4, 2.8),
    ("Sectors 1-16\nMineGRUB\n8 KB",     ACCENT2,                        1.8, 3.9, 2.0, 2.8),
    ("Sectors 17-144\nKernel chunk 1\n64 KB", GREEN,                     3.9, 3.9, 2.5, 2.8),
    ("Sectors 145-272\nKernel chunk 2\n64 KB", RGBColor(0x1A,0x8A,0x2A), 6.5, 3.9, 2.5, 2.8),
    ("Sectors 273-400\nKernel chunk 3\n64 KB", RGBColor(0x0A,0x5A,0x1A), 9.1, 3.9, 2.5, 2.8),
    ("Free\n…",                          BG_MID,                        11.7, 3.9, 1.3, 2.8),
]
for label, col, x, y, w, h in sectors:
    add_rect(s, x, y, w, h, col)
    add_rect(s, x, y, w, 0.04, ACCENT)
    add_text(s, label, x, y+0.2, w, h-0.2,
             font_size=12, bold=True, color=WHITE, align=PP_ALIGN.CENTER)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 4 — Stage 1: MBR
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Stage 1 — MBR (boot.asm)", "512 bytes to boot the OS")
footer_bar(s)

add_bullet_block(s, [
    "Loaded by BIOS at address 0x7C00",
    "Enables A20 line (fast gate 0x92)",
    "Uses Int 13h/AH=42h — extended LBA (USB/HDD compatible)",
    "Loads MineGRUB (16 sectors) at address 0x8000",
    "16-byte DAP structure specifies LBA source address",
    "Required 0xAA55 signature at end (MBR magic)",
], 0.3, 1.25, 5.8, 3.5, font_size=15)

add_code_block(s, [
    "[BITS 16]",
    "[ORG 0x7C00]",
    "",
    "STAGE2_SEG   equ 0x0800   ; -> 0x8000",
    "STAGE2_SECTS equ 16",
    "",
    "start:",
    "    cli",
    "    xor ax, ax",
    "    mov ds, ax  /  mov es, ax",
    "    mov ss, ax  /  mov sp, 0x7C00",
    "    sti",
    "    mov [boot_drive], dl",
    "",
    "    ; A20 fast gate",
    "    in  al, 0x92",
    "    or  al, 0x02",
    "    out 0x92, al",
    "",
    "    ; Load Stage2 via LBA",
    "    mov si, dap",
    "    mov ah, 0x42",
    "    int 0x13",
    "    jc  .disk_err",
    "    jmp 0x0000:0x8000",
    "",
    "dap:",
    "    db 0x10, 0",
    "    dw 16        ; 16 sectors",
    "    dw 0x0000    ; offset 0x8000",
    "    dw 0x0800    ; segment",
    "    dq 1         ; LBA start",
    "",
    "times 510-($-$$) db 0",
    "dw 0xAA55",
], 6.2, 1.2, 6.85, 5.85, font_size=10)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 5 — Stage 2: MineGRUB
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Stage 2 — MineGRUB", "Minecraft-inspired bootloader")
footer_bar(s)

add_bullet_block(s, [
    "Loaded at 0x8000 (8 KB max — 16 sectors x 512 B)",
    "Displays a Minecraft Creeper ASCII splash screen",
    "Sets VESA VBE graphics mode 0x0115",
    "   -> 800 x 600 x 24 bpp (linear framebuffer)",
    "Fills BOOT_INFO at 0x0500:",
    "   fb_addr, width, height, bpp, vesa_flag, font, drive",
    "Switches CPU to 32-bit Protected Mode (PM)",
    "Loads the kernel in 3 LBA chunks:",
    "   Chunk1 -> 0x10000  (64 KB)   LBA 17",
    "   Chunk2 -> 0x20000  (64 KB)   LBA 145",
    "   Chunk3 -> 0x30000  (64 KB)   LBA 273",
    "Jumps to kernel entry: jmp 0x10000",
], 0.3, 1.25, 5.8, 5.7, font_size=14)

add_rect(s, 6.4, 1.3, 6.6, 5.7, BG_MID)
add_text(s, "Memory map at kernel jump", 6.5, 1.35, 6.3, 0.45,
         font_size=16, bold=True, color=ACCENT)
mem_map = [
    ("0x0000 – 0x04FF", "IVT + BDA",              BG_DARK),
    ("0x0500 – 0x050F", "BOOT_INFO (fb, mode…)",   ACCENT2),
    ("0x0510 – 0x7BFF", "Free / Real-mode stack",  BG_DARK),
    ("0x7C00 – 0x7DFF", "Stage1 MBR",              ACCENT),
    ("0x8000 – 0x9FFF", "Stage2 MineGRUB",         ACCENT2),
    ("0x10000 – 0x1FFFF","Kernel chunk 1",          GREEN),
    ("0x20000 – 0x2FFFF","Kernel chunk 2",          RGBColor(0x1A,0x8A,0x2A)),
    ("0x30000 – 0x3FFFF","Kernel chunk 3",          RGBColor(0x0A,0x5A,0x1A)),
    ("0x300000 …",       "SFCML backbuffer",        ORANGE),
],
for i, (addr, label, col) in enumerate(mem_map[0]):
    add_rect(s, 6.5, 1.9 + i*0.47, 6.3, 0.44, col)
    add_text(s, addr,  6.55, 1.93 + i*0.47, 2.8, 0.38, font_size=11,
             color=YELLOW, bold=True)
    add_text(s, label, 9.4,  1.93 + i*0.47, 3.3, 0.38, font_size=11,
             color=WHITE)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 6 — Kernel C
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Kernel C — kmain.c", "Core of the operating system")
footer_bar(s)

add_rect(s, 0.3, 1.3, 5.8, 5.6, BG_MID)
add_rect(s, 0.3, 1.3, 5.8, 0.05, GREEN)
add_text(s, "Kernel architecture", 0.5, 1.35, 5.5, 0.5,
         font_size=18, bold=True, color=GREEN)
layers = [
    ("Applications (shell, apps, games)", ORANGE,   1.35, 5.4),
    ("Terminal Shell — 180+ commands",    ACCENT,   1.9,  4.8),
    ("SFCML — Windows, Graphics, Events", ACCENT2,  2.45, 4.2),
    ("libk — string, stdlib, stdio",      YELLOW,   3.0,  3.6),
    ("Drivers — PS/2, VESA, RTL8139",     RED_SOFT, 3.55, 3.0),
    ("kernel/crt0.asm — entry point",     GREEN,    4.1,  2.4),
]
for label, col, y, _ in layers:
    add_rect(s, 0.5, y, 5.4, 0.55, col)
    add_text(s, label, 0.5, y, 5.4, 0.55,
             font_size=13, bold=True, color=WHITE, align=PP_ALIGN.CENTER)

add_rect(s, 6.4, 1.3, 6.6, 5.6, BG_MID)
add_rect(s, 6.4, 1.3, 6.6, 0.05, ACCENT)
add_text(s, "Build & Link", 6.6, 1.35, 6.3, 0.5,
         font_size=18, bold=True, color=ACCENT)
add_bullet_block(s, [
    "CC = gcc -m32 -ffreestanding -nostdlib",
    "   -nostartfiles -fno-pic -fno-pie -O2",
    "   -Ilibc/include -Isfcml/include -Inet",
    "",
    "Linker script kernel.ld:",
    "   . = 0x10000  (physical load address)",
    "   .text -> .rodata -> .data -> .bss",
    "",
    "Linked objects:",
    "   crt0.o + kmain.o",
    "   libsfcml.a (window, graphics, events)",
    "   libk.a (string, stdlib, stdio)",
    "   net.o  (RTL8139, DHCP, ARP)",
    "",
    "Final output:",
    "   kernel.elf -> objcopy -> kernel.bin",
    "   Current size: ~139 KB",
], 6.5, 1.9, 6.3, 4.9, font_size=13)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 7 — Libraries
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Libraries — 100% From Scratch", "libk + libSFCML + net")
footer_bar(s)

cols = [
    ("libk (minimal libc)", ACCENT, 0.3, [
        "string.h: memcpy, memset, strlen, strcmp…",
        "stdlib.h: itoa, atoi, basic malloc",
        "stdio.h:  sprintf, snprintf",
        "Compiled into libk.a (ar rcs)",
    ]),
    ("libSFCML (graphics)", ACCENT2, 4.6, [
        "sfcml_createWindow() — VESA framebuffer",
        "sfcml_drawRect/Circle/Line()",
        "sfcml_drawText() — embedded bitmap font",
        "sfcml_present() — double-buffering flip",
        "sfcml_pollEvent() — PS/2 kbd + mouse",
        "Compiled into libsfcml.a",
    ]),
    ("net (networking)", GREEN, 8.9, [
        "RTL8139 PCI driver (direct I/O ports)",
        "Ethernet frames, ARP, IPv4",
        "UDP, DHCP client",
        "Kernel-side virtual sockets",
        "dhcp_request() -> auto IP assignment",
    ]),
]
for title, col, x, items in cols:
    add_rect(s, x, 1.3, 4.1, 5.7, BG_MID)
    add_rect(s, x, 1.3, 4.1, 0.05, col)
    add_text(s, title, x+0.1, 1.35, 3.9, 0.55,
             font_size=16, bold=True, color=col)
    add_bullet_block(s, items, x+0.1, 1.95, 3.8, 5.0,
                     font_size=13, bullet="• ")

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 8 — Graphics
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "VESA Graphical Interface", "800 x 600 x 24 bpp — Linear Framebuffer")
footer_bar(s)

add_bullet_block(s, [
    "VBE mode 0x0115 — 800x600 true-color 24bpp",
    "Physical linear framebuffer — direct pointer access",
    "Double-buffering: backbuffer at 0x300000 (1.44 MB)",
    "   sfcml_clear() -> sfcml_present() = atomic flip",
    "8x8 px bitmap font embedded in the kernel",
    "Primitives: pixel, rect, circle, line, text, triangle",
    "Window manager: overlapping windows with z-order",
    "PS/2 mouse — 3-byte packets, absolute via VMMouse",
    "PS/2 keyboard — Set 1 scancodes, AZERTY layout",
    "VMMouse backdoor (port 0x5658) — no grab needed",
], 0.3, 1.3, 5.8, 5.7, font_size=15)

add_rect(s, 6.5, 1.3, 6.5, 5.7, BG_MID)
add_text(s, "Render pipeline", 6.7, 1.35, 6.2, 0.5,
         font_size=18, bold=True, color=ACCENT)

steps = [
    ("1. App draws into the backbuffer",    ACCENT2),
    ("   sfcml_drawRect / drawText …",      BG_DARK),
    ("2. sfcml_present()",                   ACCENT),
    ("   memcpy(fb, back, pitch x height)", BG_DARK),
    ("3. VESA displays the framebuffer",     GREEN),
    ("   Resolution: 800x600 px",            BG_DARK),
    ("   BPP: 24 bits (R G B)",              BG_DARK),
    ("   Pitch: 800 x 3 = 2400 bytes",      BG_DARK),
    ("4. PS/2 kbd -> scancode Set 1",        ORANGE),
    ("5. VMMouse port 0x5658 -> abs XY",     RED_SOFT),
    ("6. Window manager dispatches event",   YELLOW),
],
for i, (text, col) in enumerate(steps[0]):
    add_rect(s, 6.6, 1.95 + i*0.42, 6.1, 0.4, col)
    add_text(s, text, 6.7, 1.97 + i*0.42, 5.9, 0.36,
             font_size=12, color=WHITE)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 9 — Terminal & Commands
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Terminal — 180+ Commands", "Interactive shell, Linux-style")
footer_bar(s)

categories = [
    ("Filesystem", ACCENT, [
        "ls, cd, pwd, cat, echo, clear",
        "mkdir, rm, cp, mv, touch, chmod",
        "nano / vi — file editor (VFS)",
        "find, stat, file, tree",
    ]),
    ("Text tools", GREEN, [
        "grep, sed, awk, cut, sort, uniq",
        "wc, head, tail, diff, tr, tee",
        "base64, xxd, hexdump",
        "md5sum, sha256sum",
    ]),
    ("Math & Calc", YELLOW, [
        "bc / expr  — arithmetic",
        "factor     — prime factors",
        "primes     — prime numbers",
        "seq, yes, tee, xargs",
    ]),
    ("Network", ORANGE, [
        "ping, ifconfig, netstat",
        "dhclient, arp, route",
        "nslookup, traceroute, ss",
        "nc, tcpdump, ethtool",
    ]),
    ("System", ACCENT2, [
        "lspci, lsusb, dmesg",
        "mount, umount, fdisk",
        "systemctl, journalctl",
        "ps, top, kill, free",
    ]),
    ("Fun & Games", RED_SOFT, [
        "cowsay, figlet, lolcat",
        "sl, fortune, matrix",
        "snake, rtype, pong",
        "screensaver, logo",
    ]),
]
positions = [(0.2, 1.3), (2.45, 1.3), (4.7, 1.3), (6.95, 1.3), (9.2, 1.3), (11.45, 1.3)]
for (x, y), (title, col, items) in zip(positions, categories):
    add_rect(s, x, y, 2.1, 5.7, BG_MID)
    add_rect(s, x, y, 2.1, 0.05, col)
    add_text(s, title, x+0.05, y+0.08, 2.0, 0.55,
             font_size=12, bold=True, color=col)
    add_bullet_block(s, items, x+0.05, y+0.65, 2.0, 4.9,
                     font_size=11, bullet="", color=LIGHT)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 10 — Applications
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Built-in Applications", "Window manager + Apps")
footer_bar(s)

apps = [
    ("Terminal\nShell",    GREEN,    "180+ commands\nLinux-like\nPS/2 keyboard"),
    ("File\nManager",      ACCENT,   "In-RAM VFS\nNavigation\nFile viewer"),
    ("Web\nBrowser",       ORANGE,   "Simulated HTTP\nBasic HTML\nrendering"),
    ("Paint\nApp",         RED_SOFT, "Freehand draw\nColors\nTools"),
    ("Snake\nGame",        YELLOW,   "PS/2 keyboard\nScore\nGrid 36x27"),
    ("R-Type\nGame",       ACCENT2,  "Shoot-em-up\nEnemies + AI\nVMMouse"),
    ("Pong\nGame",         LIGHT,    "Player vs AI\nPhysics\nScoreboard"),
    ("Text\nEditor",       RGBColor(0xFF,0x6A,0xFF), "nano/vi style\nVFS save\nAZERTY"),
]
xs = [0.25, 1.9, 3.55, 5.2, 6.85, 8.5, 10.15, 11.8]
for x, (name, col, desc) in zip(xs, apps):
    add_rect(s, x, 1.35, 1.55, 1.55, col)
    add_text(s, name, x, 1.35, 1.55, 1.55,
             font_size=14, bold=True, color=WHITE, align=PP_ALIGN.CENTER)
    add_rect(s, x, 3.0, 1.55, 2.5, BG_MID)
    add_text(s, desc, x+0.05, 3.05, 1.45, 2.4,
             font_size=11, color=LIGHT, align=PP_ALIGN.LEFT)

add_rect(s, 0.2, 5.7, 12.9, 1.1, BG_MID)
add_text(s, "All applications use libSFCML for rendering and keyboard/mouse events",
         0.3, 5.8, 12.7, 0.45, font_size=14, color=LIGHT)
add_text(s, "Integrated window manager: focus, drag & drop, multi-window, z-order",
         0.3, 6.2, 12.7, 0.45, font_size=14, color=ACCENT)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 11 — Networking
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Network Stack", "RTL8139 + Ethernet + IPv4 + DHCP")
footer_bar(s)

add_bullet_block(s, [
    "RTL8139 driver written from scratch (direct PCI access)",
    "No wrappers — raw read/write of I/O registers",
    "Full network stack:",
    "   Ethernet II -> ARP -> IPv4 -> UDP -> DHCP",
    "DHCP DISCOVER -> OFFER -> REQUEST -> ACK",
    "IP address auto-assigned at boot",
    "ARP table in memory (mac <-> IP)",
    "Kernel-side virtual UDP sockets",
    "ping — sends ICMP Echo Request",
    "curl — basic HTTP GET over simulated TCP",
], 0.3, 1.3, 5.8, 5.7, font_size=15)

add_code_block(s, [
    "/* net/net.c — simplified DHCP */",
    "",
    "void dhcp_request(uint8_t *mac) {",
    "    uint8_t pkt[300];",
    "    memset(pkt, 0, sizeof(pkt));",
    "",
    "    /* Ethernet header */",
    "    memset(pkt, 0xFF, 6);     /* dst: broadcast */",
    "    memcpy(pkt+6, mac, 6);   /* src: our MAC   */",
    "    pkt[12] = 0x08; pkt[13] = 0x00; /* IPv4 */",
    "",
    "    /* IP + UDP headers ... */",
    "",
    "    /* DHCP DISCOVER payload */",
    "    pkt[42] = 1;   /* BOOTREQUEST */",
    "    pkt[43] = 1;   /* Ethernet    */",
    "    pkt[44] = 6;   /* MAC length  */",
    "    /* magic cookie */",
    "    pkt[278] = 99; pkt[279] = 130;",
    "    pkt[280] = 83; pkt[281] = 99;",
    "",
    "    rtl8139_send(pkt, sizeof(pkt));",
    "}",
], 6.3, 1.25, 6.8, 5.85, font_size=10)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 12 — Build System
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Build System — Makefile", "Full compilation with a single command")
footer_bar(s)

add_code_block(s, [
    "# Freestanding 32-bit C compilation",
    "CC     = gcc",
    "CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles \\",
    "         -fno-pic -fno-pie -O2 -Wall -Wextra \\",
    "         -Ilibc/include -Isfcml/include -Inet",
    "",
    "# Kernel assembly",
    "$(BUILD)/kernel/crt0.o: kernel/crt0.asm",
    "    $(AS) -f elf32 -o $@ $<",
    "",
    "# Final link",
    "$(BUILD)/kernel.elf: crt0.o kmain.o $(NET_OBJ) \\",
    "                     libsfcml.a libk.a",
    "    ld -m elf_i386 -T kernel/kernel.ld -o $@ ...",
    "",
    "# Disk image (3 chunks)",
    "$(BUILD)/myos.img: boot.bin minegrub.bin kernel.bin",
    "    dd if=kernel.bin bs=512 count=128 seek=17  ...",
    "    dd if=kernel.bin bs=512 skip=128 seek=145  ...",
    "    dd if=kernel.bin bs=512 skip=256 seek=273  ...",
    "",
    "# Launch in QEMU",
    "run: all",
    "    qemu-system-i386 -drive file=myos.img,format=raw \\",
    "        -vga std -m 128M -netdev user,id=net0 \\",
    "        -device rtl8139,netdev=net0",
], 0.3, 1.25, 7.0, 5.85, font_size=11)

add_rect(s, 7.5, 1.3, 5.6, 5.7, BG_MID)
add_text(s, "Make targets", 7.7, 1.35, 5.3, 0.5,
         font_size=18, bold=True, color=ACCENT)
targets = [
    ("make all",       "Full build (boot + stage2 + kernel)", ACCENT),
    ("make run",       "Launch QEMU with myos.img",           GREEN),
    ("make run-asm",   "Launch pure ASM kernel",              ACCENT2),
    ("make debug",     "QEMU + remote GDB (port 1234)",       YELLOW),
    ("make debug-asm", "Debug ASM kernel via GDB",            ORANGE),
    ("make libs",      "Compile libk.a + libsfcml.a only",    LIGHT),
    ("make clean",     "Delete entire build/ directory",      RED_SOFT),
    ("make usb",       "Flash to USB drive (DEV=/dev/sdX)",   ACCENT2),
]
for i, (cmd, desc, col) in enumerate(targets):
    add_rect(s, 7.55, 1.9 + i*0.58, 5.5, 0.55, col)
    add_text(s, cmd, 7.6, 1.92 + i*0.58, 1.9, 0.48,
             font_size=13, bold=True, color=WHITE, align=PP_ALIGN.LEFT)
    add_text(s, desc, 9.6, 1.92 + i*0.58, 3.4, 0.48,
             font_size=12, color=WHITE)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 13 — Challenges & Solutions
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Technical Challenges & Solutions", "Problems encountered and solved")
footer_bar(s)

challenges = [
    ("Kernel size limit\n128 KB -> 192 KB",   ORANGE,
     "Kernel exceeded 2 chunks x 64 KB.\nSolution: 3rd LBA chunk at 273 -> 0x30000"),
    ("Protected mode, no libc\n-ffreestanding", ACCENT,
     "No printf, malloc, string.h.\nSolution: libk 100% from scratch"),
    ("VESA VBE 800x600\nPS/2 integration",     GREEN,
     "Keyboard (scancode Set1) + Mouse (3-byte packets).\nVMMouse backdoor port 0x5658 for absolute coords"),
    ("RTL8139 PCI driver\nDHCP from scratch",  RED_SOFT,
     "Direct I/O register access.\nManual Ethernet/UDP/DHCP stack"),
    ("USB boot vs floppy\nCHS vs LBA",         ACCENT2,
     "CHS limited to 8 GB, USB needs LBA.\nInt 13h/AH=42h + 16-byte DAP"),
    ("Bare-metal debugging\nNo printf",         YELLOW,
     "QEMU -s -S + GDB remote on port 1234.\nMessages via VGA text mode 80x25"),
]
positions2 = [(0.25, 1.3), (4.45, 1.3), (8.65, 1.3),
              (0.25, 4.05), (4.45, 4.05), (8.65, 4.05)]
for (x, y), (title, col, sol) in zip(positions2, challenges):
    add_rect(s, x, y, 4.0, 2.5, BG_MID)
    add_rect(s, x, y, 4.0, 0.05, col)
    add_text(s, title, x+0.1, y+0.1, 3.8, 0.65,
             font_size=14, bold=True, color=col)
    add_text(s, sol, x+0.1, y+0.8, 3.8, 1.6,
             font_size=12, color=LIGHT)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 14 — Results & Demo
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
header_bar(s, "Results & Demonstration", "What MyOS can do today")
footer_bar(s)

add_rect(s, 0.3, 1.3, 12.7, 0.65, BG_MID)
add_text(s, "make run  ->  qemu-system-i386 boots MyOS in ~2 seconds",
         0.5, 1.38, 12.3, 0.48, font_size=16, color=GREEN, bold=True)

stats = [
    ("~139 KB", "Kernel size",       ACCENT),
    ("180+",    "Commands",           GREEN),
    ("11",      "Applications",       ORANGE),
    ("800×600", "Resolution",         ACCENT2),
    ("24 bpp",  "Color depth",        YELLOW),
    ("RTL8139", "Network",            RED_SOFT),
    ("0",       "External libs",      RGBColor(0x80,0xFF,0x80)),
    ("3",       "Kernel chunks",      LIGHT),
]
for i, (val, label, col) in enumerate(stats):
    x = 0.3 + (i % 4) * 3.2
    y = 2.15 + (i // 4) * 1.8
    add_rect(s, x, y, 3.0, 1.55, col)
    add_text(s, val,   x, y+0.05, 3.0, 0.9,
             font_size=30, bold=True, color=WHITE, align=PP_ALIGN.CENTER)
    add_text(s, label, x, y+0.9,  3.0, 0.6,
             font_size=13, color=WHITE, align=PP_ALIGN.CENTER)

add_rect(s, 0.3, 5.75, 12.7, 1.05, BG_MID)
add_text(s,
    "Live demo:  Terminal -> Linux commands  |  Paint  |  Snake / R-Type / Pong  |  ping / ifconfig",
    0.5, 5.85, 12.5, 0.45, font_size=15, color=ACCENT)
add_text(s,
    "Boot QEMU: make run   |   GDB debug: make debug   |   Pure ASM kernel: make run-asm",
    0.5, 6.28, 12.5, 0.45, font_size=13, color=LIGHT)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDE 15 — Conclusion
# ═══════════════════════════════════════════════════════════════════════════════
s = prs.slides.add_slide(BLANK_LAYOUT)
slide_bg(s, BG_DARK)
add_rect(s, 0, 0, 13.33, 2.0, BG_MID)
add_rect(s, 0, 1.95, 13.33, 0.08, ACCENT)
add_text(s, "Conclusion", 0.5, 0.15, 12.3, 1.3,
         font_size=55, bold=True, color=WHITE, align=PP_ALIGN.CENTER)
add_text(s, "MyOS — x86 operating system built entirely from scratch", 0.5, 1.35, 12.3, 0.55,
         font_size=18, color=ACCENT, align=PP_ALIGN.CENTER, italic=True)

add_bullet_block(s, [
    "Complete bare-metal OS in NASM assembly + freestanding C",
    "MineGRUB bootloader: stage1 MBR -> stage2 VESA -> kernel PM",
    "Modular kernel: drivers, window manager, shell, networking",
    "180+ Linux-like commands in an interactive terminal",
    "11 applications with VESA 800x600 graphical interface",
    "Full network stack: RTL8139, Ethernet, ARP, IPv4, DHCP",
    "3 embedded games: Snake, R-Type, Pong — all from scratch",
    "0 external libraries — hard constraint of the project",
    "Reproducible build: make run boots in ~2s in QEMU",
], 0.5, 2.2, 7.8, 4.6, font_size=16)

add_rect(s, 8.8, 2.1, 4.3, 4.7, BG_MID)
add_rect(s, 8.8, 2.1, 4.3, 0.06, ACCENT2)
add_text(s, "Key skills", 8.9, 2.17, 4.1, 0.55,
         font_size=16, bold=True, color=ACCENT2)
add_bullet_block(s, [
    "x86 low-level programming",
    "OS architecture (ring 0)",
    "Physical memory management",
    "Hardware drivers",
    "Network protocols",
    "Complex build systems",
    "GDB + QEMU debugging",
    "Working under hard constraints",
], 8.9, 2.75, 4.1, 4.0, font_size=14)

add_rect(s, 0, 6.85, 13.33, 0.65, RGBColor(0x05,0x10,0x1A))
add_text(s,
    "MyOS  ·  Epitech Project  ·  2026  ·  kevin.landbeck68.sn@gmail.com",
    0.3, 6.9, 12.7, 0.48, font_size=13, color=ACCENT, align=PP_ALIGN.CENTER)

# ── Save ──────────────────────────────────────────────────────────────────────
out = r"c:\Users\towoga\Downloads\myos_asm (1)\myos_asm\MyOS_Presentation_EN.pptx"
prs.save(out)
print(f"[OK] Saved: {out}")
