#include "sfcml.h"
#include "stdint.h"

#define BOOT_INFO  ((volatile uint8_t*)0x0500)
#define BI_WIDTH   (*((volatile uint16_t*)(BOOT_INFO + 4)))
#define BI_HEIGHT  (*((volatile uint16_t*)(BOOT_INFO + 6)))

/* ============================================================
 * Clavier PS/2 bare-metal
 * Port 0x60 = data, 0x64 = status
 * ============================================================ */

static inline uint8_t _inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void _outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t _kbd_status(void) { return _inb(0x64); }
static inline uint8_t _kbd_data(void)   { return _inb(0x60); }

static void _wait_wr(void) { /* attend que l'input buffer soit libre */
    for (int i = 0; i < 100000 && (_kbd_status() & 0x02); i++);
}
static void _wait_rd(void) { /* attend qu'une donnee soit disponible */
    for (int i = 0; i < 100000 && !(_kbd_status() & 0x01); i++);
}

/* ============================================================
 * Souris PS/2
 * ============================================================ */
static void _evt_push(sfcml_Event e); /* declaration anticipee */

static int     _mx = 400, _my = 300, _mbtn = 0;
static int     _mp = 0;        /* phase du paquet (0-2) */
static uint8_t _mpkt[3];

static void _mouse_handle(uint8_t data) {
    _mpkt[_mp] = data;
    /* Synchronisation : bit 3 du premier octet doit etre 1 */
    if (_mp == 0 && !(data & 0x08)) return;
    if (++_mp < 3) return;
    _mp = 0;

    uint8_t flags = _mpkt[0];
    int dx = (int)_mpkt[1] - ((flags & 0x10) ? 256 : 0);
    int dy = (int)_mpkt[2] - ((flags & 0x20) ? 256 : 0);
    _mx += dx;
    _my -= dy; /* Y inverse en PS/2 */
    int _scr_w = (int)BI_WIDTH;
    int _scr_h = (int)BI_HEIGHT;
    if (_scr_w <= 0) _scr_w = 800;
    if (_scr_h <= 0) _scr_h = 600;
    if (_mx <  0) _mx = 0;
    if (_mx >= _scr_w) _mx = _scr_w - 1;
    if (_my <  0) _my = 0;
    if (_my >= _scr_h) _my = _scr_h - 1;

    sfcml_Event em = {0};
    em.type = SFCML_EVT_MOUSE_MOVED;
    em.mouse.x = _mx; em.mouse.y = _my;
    _evt_push(em);

    int new_btn = flags & 0x03;
    for (int b = 0; b < 2; b++) {
        if ((new_btn ^ _mbtn) & (1 << b)) {
            sfcml_Event eb = {0};
            eb.type = (new_btn & (1 << b)) ?
                      SFCML_EVT_MOUSE_PRESSED : SFCML_EVT_MOUSE_RELEASED;
            eb.mouse.button = b;
            eb.mouse.x = _mx; eb.mouse.y = _my;
            _evt_push(eb);
        }
    }
    _mbtn = new_btn;
}

/* Table scancode → keycode (set 1, make code) */
static const sfcml_KeyCode _sc_table[128] = {
    /* 0x00 */ SFCML_KEY_NONE,
    /* 0x01 */ SFCML_KEY_ESCAPE,
    /* 0x02 */ SFCML_KEY_1,
    /* 0x03 */ SFCML_KEY_2,
    /* 0x04 */ SFCML_KEY_3,
    /* 0x05 */ SFCML_KEY_4,
    /* 0x06 */ SFCML_KEY_5,
    /* 0x07 */ SFCML_KEY_6,
    /* 0x08 */ SFCML_KEY_7,
    /* 0x09 */ SFCML_KEY_8,
    /* 0x0A */ SFCML_KEY_9,
    /* 0x0B */ SFCML_KEY_0,
    /* 0x0C */ SFCML_KEY_NONE, /* - */
    /* 0x0D */ SFCML_KEY_NONE, /* = */
    /* 0x0E */ SFCML_KEY_BACKSPACE,
    /* 0x0F */ SFCML_KEY_TAB,
    /* 0x10 */ SFCML_KEY_Q,
    /* 0x11 */ SFCML_KEY_W,
    /* 0x12 */ SFCML_KEY_E,
    /* 0x13 */ SFCML_KEY_R,
    /* 0x14 */ SFCML_KEY_T,
    /* 0x15 */ SFCML_KEY_Y,
    /* 0x16 */ SFCML_KEY_U,
    /* 0x17 */ SFCML_KEY_I,
    /* 0x18 */ SFCML_KEY_O,
    /* 0x19 */ SFCML_KEY_P,
    /* 0x1A */ SFCML_KEY_NONE, /* [ */
    /* 0x1B */ SFCML_KEY_NONE, /* ] */
    /* 0x1C */ SFCML_KEY_RETURN,
    /* 0x1D */ SFCML_KEY_NONE, /* Ctrl gauche */
    /* 0x1E */ SFCML_KEY_A,
    /* 0x1F */ SFCML_KEY_S,
    /* 0x20 */ SFCML_KEY_D,
    /* 0x21 */ SFCML_KEY_F,
    /* 0x22 */ SFCML_KEY_G,
    /* 0x23 */ SFCML_KEY_H,
    /* 0x24 */ SFCML_KEY_J,
    /* 0x25 */ SFCML_KEY_K,
    /* 0x26 */ SFCML_KEY_L,
    /* 0x27 */ SFCML_KEY_NONE, /* ; */
    /* 0x28 */ SFCML_KEY_NONE, /* ' */
    /* 0x29 */ SFCML_KEY_NONE, /* ` */
    /* 0x2A */ SFCML_KEY_NONE, /* Shift gauche */
    /* 0x2B */ SFCML_KEY_NONE, /* \ */
    /* 0x2C */ SFCML_KEY_Z,
    /* 0x2D */ SFCML_KEY_X,
    /* 0x2E */ SFCML_KEY_C,
    /* 0x2F */ SFCML_KEY_V,
    /* 0x30 */ SFCML_KEY_B,
    /* 0x31 */ SFCML_KEY_N,
    /* 0x32 */ SFCML_KEY_M,
    /* 0x33 */ SFCML_KEY_NONE, /* , */
    /* 0x34 */ SFCML_KEY_NONE, /* . */
    /* 0x35 */ SFCML_KEY_NONE, /* / */
    /* 0x36 */ SFCML_KEY_NONE, /* Shift droit */
    /* 0x37 */ SFCML_KEY_NONE,
    /* 0x38 */ SFCML_KEY_NONE, /* Alt */
    /* 0x39 */ SFCML_KEY_SPACE,
    /* 0x3A */ SFCML_KEY_NONE, /* Caps Lock */
    /* 0x3B */ SFCML_KEY_F1,
    /* 0x3C */ SFCML_KEY_F2,
    /* 0x3D */ SFCML_KEY_F3,
    /* 0x3E */ SFCML_KEY_F4,
    /* 0x3F */ SFCML_KEY_F5,
    /* 0x40 */ SFCML_KEY_F6,
    /* 0x41 */ SFCML_KEY_F7,
    /* 0x42 */ SFCML_KEY_F8,
    /* 0x43 */ SFCML_KEY_F9,
    /* 0x44 */ SFCML_KEY_F10,
    [0x57]  = SFCML_KEY_F11,
    [0x58]  = SFCML_KEY_F12,
};

/* Etat modificateurs */
static uint8_t _shift = 0;
static uint8_t _ctrl  = 0;
static uint8_t _alt   = 0;
static uint8_t _ext   = 0;

/* File d'evenements circulaire */
#define EVT_BUF 32
static sfcml_Event _evt_buf[EVT_BUF];
static int _evt_head = 0, _evt_tail = 0;

static void _evt_push(sfcml_Event e) {
    int next = (_evt_tail + 1) % EVT_BUF;
    if (next != _evt_head) { _evt_buf[_evt_tail] = e; _evt_tail = next; }
}

static int _evt_pop(sfcml_Event* e) {
    if (_evt_head == _evt_tail) return 0;
    *e = _evt_buf[_evt_head];
    _evt_head = (_evt_head + 1) % EVT_BUF;
    return 1;
}

/* Conversion scancode → caractere ASCII (AZERTY simplifie) */
static const char _ascii_lo[58] = {
    0,0,'&','e','"','\'','(','-','e','_','c','a',')',
    '=',0,0,'a','z','e','r','t','y','u','i','o','p',
    '^','$','\n',0,'q','s','d','f','g','h','j','k','l',
    'm',0,'`',0,'*','w','x','c','v','b','n',',',';',
    ':','!',0,0,0,' '
};

static const char _ascii_hi[58] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','+',
    '=',0,0,'A','Z','E','R','T','Y','U','I','O','P',
    0,0,'\n',0,'Q','S','D','F','G','H','J','K','L',
    'M',0,0,0,'*','W','X','C','V','B','N','?','.',
    '/',0,0,0,0,' '
};

static void _process_scancode(uint8_t sc) {
    if (sc == 0xE0) { _ext = 1; return; }

    int released = (sc & 0x80) != 0;
    uint8_t code = sc & 0x7F;

    if (_ext) {
        _ext = 0;
        if (code == 0x1D) { _ctrl = !released; return; }
        if (code == 0x38) { _alt  = !released; return; }
        if (!released) {
            sfcml_KeyCode kc = SFCML_KEY_NONE;
            if      (code == 0x48) kc = SFCML_KEY_UP;
            else if (code == 0x50) kc = SFCML_KEY_DOWN;
            else if (code == 0x4B) kc = SFCML_KEY_LEFT;
            else if (code == 0x4D) kc = SFCML_KEY_RIGHT;
            if (kc) {
                sfcml_Event e = {0};
                e.type = SFCML_EVT_KEY_PRESSED;
                e.key.code = kc; e.key.shift=_shift; e.key.ctrl=_ctrl; e.key.alt=_alt;
                _evt_push(e);
            }
        }
        return;
    }

    if (code == 0x2A || code == 0x36) { _shift = !released; return; }
    if (code == 0x1D) { _ctrl  = !released; return; }
    if (code == 0x38) { _alt   = !released; return; }

    sfcml_Event e = {0};
    e.type = released ? SFCML_EVT_KEY_RELEASED : SFCML_EVT_KEY_PRESSED;
    e.key.code  = (code < 128) ? _sc_table[code] : SFCML_KEY_NONE;
    e.key.shift = _shift;
    e.key.ctrl  = _ctrl;
    e.key.alt   = _alt;
    _evt_push(e);

    if (!released && code < 58) {
        char ch = _shift ? _ascii_hi[code] : _ascii_lo[code];
        if (ch) {
            sfcml_Event et = { .type = SFCML_EVT_TEXT, .text = { ch } };
            _evt_push(et);
        }
    }
}

/* Lit le clavier ET la souris PS/2 et remplit la file */
static void _kbd_poll(void) {
    while (1) {
        uint8_t st = _kbd_status();
        if (!(st & 0x01)) break;          /* rien a lire */
        uint8_t d = _kbd_data();
        if (st & 0x20) _mouse_handle(d); /* bit5=donnee souris (aux) */
        else           _process_scancode(d);
    }
}

int sfcml_pollEvent(sfcml_Window* win, sfcml_Event* evt) {
    (void)win;
    _kbd_poll();
    return _evt_pop(evt);
}

void sfcml_waitEvent(sfcml_Window* win, sfcml_Event* evt) {
    while (!sfcml_pollEvent(win, evt)) {
        __asm__ volatile("pause");
    }
}

int sfcml_isKeyPressed(sfcml_KeyCode key) {
    (void)key;
    return 0;
}

/* ============================================================
 * Souris - initialisation et accesseurs
 * ============================================================ */
void sfcml_mouseInit(void) {
    _wait_wr(); _outb(0x64, 0xA8);  /* activer port auxiliaire */
    while (_kbd_status() & 0x01) _inb(0x60); /* vider buffer */
    /* Set defaults */
    _wait_wr(); _outb(0x64, 0xD4);
    _wait_wr(); _outb(0x60, 0xF6);
    _wait_rd(); if (_kbd_status() & 0x01) _inb(0x60); /* ACK */
    /* Enable data reporting */
    _wait_wr(); _outb(0x64, 0xD4);
    _wait_wr(); _outb(0x60, 0xF4);
    _wait_rd(); if (_kbd_status() & 0x01) _inb(0x60); /* ACK */
}

int sfcml_getMouseX(void)         { return _mx; }
int sfcml_getMouseY(void)         { return _my; }
int sfcml_getMouseButton(int btn) { return (_mbtn >> btn) & 1; }

/* ============================================================
 * Chronometre (utilise le compteur PIT via port 0x40)
 * On utilise les ticks BIOS stoctes a 0x046C (dword) en RAM basse
 * ============================================================ */
#define BIOS_TICKS (*(volatile uint32_t*)0x046C)
#define TICKS_PER_SEC 18   /* ~18.2 Hz */

void sfcml_clockStart(sfcml_Clock* clk) {
    clk->start_ticks = BIOS_TICKS;
}

uint32_t sfcml_clockElapsed(sfcml_Clock* clk) {
    uint32_t diff = BIOS_TICKS - clk->start_ticks;
    return diff * 1000 / TICKS_PER_SEC;
}

void sfcml_clockRestart(sfcml_Clock* clk) {
    clk->start_ticks = BIOS_TICKS;
}
