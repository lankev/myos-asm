#include "sfcml.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"

/* Boot info layout a 0x0500 (rempli par MineGRUB) */
#define BOOT_INFO ((volatile uint8_t*)0x0500)
#define BI_FB_ADDR  (*((volatile uint32_t*)(BOOT_INFO + 0)))
#define BI_WIDTH    (*((volatile uint16_t*)(BOOT_INFO + 4)))
#define BI_HEIGHT   (*((volatile uint16_t*)(BOOT_INFO + 6)))
#define BI_BPP      (*((volatile uint8_t* )(BOOT_INFO + 8)))
#define BI_VESA     (*((volatile uint8_t* )(BOOT_INFO + 9)))
#define BI_FONT     (*((volatile uint32_t*)(BOOT_INFO + 10)))
#define BI_DRIVE    (*((volatile uint8_t* )(BOOT_INFO +14)))
#define BI_MEMLO_KB (*((volatile uint16_t*)(BOOT_INFO +16)))
#define BI_MEMHI_KB (*((volatile uint16_t*)(BOOT_INFO +18)))

static sfcml_Window _win_singleton;
static int _initialized = 0;

int sfcml_init(void) {
    return BI_VESA ? 1 : 0;
}

void sfcml_shutdown(void) {
    _initialized = 0;
}

sfcml_Window* sfcml_createWindow(const char* title) {
    (void)title;
    sfcml_Window* win = &_win_singleton;

    /* Backbuffer en RAM etendue (3 Mo) — hors zone kernel/BIOS */
    win->back = (uint8_t*)0x300000;

    if (BI_VESA) {
        win->fb     = (uint8_t*)BI_FB_ADDR;
        win->width  = BI_WIDTH;
        win->height = BI_HEIGHT;
        win->bpp    = BI_BPP;
        win->pitch  = BI_WIDTH * (BI_BPP / 8);
        win->font   = (uint8_t*)BI_FONT;
    } else {
        win->fb     = NULL;
        win->width  = 640;
        win->height = 480;
        win->bpp    = 24;
        win->pitch  = 640 * 3;
        win->font   = (uint8_t*)BI_FONT;
    }
    win->open = 1;
    _initialized = 1;
    return win;
}

void sfcml_destroyWindow(sfcml_Window* win) {
    if (win) win->open = 0;
}

int sfcml_isOpen(const sfcml_Window* win) {
    return win && win->open;
}

void sfcml_close(sfcml_Window* win) {
    if (win) win->open = 0;
}

/* Ecrit un pixel dans le backbuffer (pas dans le FB) */
static inline void _put(sfcml_Window* win, int x, int y, sfcml_Color c) {
    if (!win->back) return;
    if ((unsigned)x >= win->width || (unsigned)y >= win->height) return;
    uint8_t* p = win->back + (uint32_t)y * win->pitch + (uint32_t)x * 3;
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;
}

/* Efface le backbuffer */
void sfcml_clear(sfcml_Window* win, sfcml_Color c) {
    if (!win->back) return;
    uint32_t total = win->pitch * win->height;
    uint8_t* p = win->back;
    for (uint32_t i = 0; i < total; i += 3) {
        p[i]   = c.b;
        p[i+1] = c.g;
        p[i+2] = c.r;
    }
}

/* Copie le backbuffer vers le framebuffer VESA */
void sfcml_present(sfcml_Window* win) {
    if (!win->fb || !win->back) return;
    memcpy(win->fb, win->back, win->pitch * win->height);
}

sfcml_Color sfcml_blend(sfcml_Color src, sfcml_Color dst) {
    if (src.a == 255) return src;
    if (src.a == 0)   return dst;
    uint8_t a = src.a;
    uint8_t ia = 255 - a;
    return (sfcml_Color){
        (uint8_t)((src.r * a + dst.r * ia) >> 8),
        (uint8_t)((src.g * a + dst.g * ia) >> 8),
        (uint8_t)((src.b * a + dst.b * ia) >> 8),
        255
    };
}

/* Pixel de base : expose pour graphics.c */
void sfcml_drawPixel(sfcml_Window* win, int x, int y, sfcml_Color c) {
    _put(win, x, y, c);
}
