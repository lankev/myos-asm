#ifndef _SFCML_H
#define _SFCML_H

/*
 * SFCML - Simple Fast C Media Library
 * Bibliotheque graphique bare-metal pour MyOS
 * Inspiree de SFML, construite sur la libc custom
 *
 * Utilise le framebuffer VESA 640x480x24bpp configure par MineGRUB.
 * Boot info disponible a l'adresse physique 0x0500 :
 *   [+0  dword] adresse physique du framebuffer
 *   [+4  word ] largeur
 *   [+6  word ] hauteur
 *   [+8  byte ] bits par pixel
 *   [+9  byte ] 1 si VESA actif
 *   [+10 dword] adresse police BIOS 8x8
 */

#include "stdint.h"
#include "stddef.h"

/* ============================================================
 * Couleur
 * ============================================================ */
typedef struct {
    uint8_t r, g, b, a;
} sfcml_Color;

static inline sfcml_Color sfcml_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (sfcml_Color){ r, g, b, 255 };
}
static inline sfcml_Color sfcml_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (sfcml_Color){ r, g, b, a };
}

#define SFCML_BLACK     sfcml_rgb(0,   0,   0  )
#define SFCML_WHITE     sfcml_rgb(255, 255, 255)
#define SFCML_RED       sfcml_rgb(255, 0,   0  )
#define SFCML_GREEN     sfcml_rgb(0,   200, 0  )
#define SFCML_BLUE      sfcml_rgb(0,   0,   200)
#define SFCML_YELLOW    sfcml_rgb(255, 255, 0  )
#define SFCML_CYAN      sfcml_rgb(0,   255, 255)
#define SFCML_MAGENTA   sfcml_rgb(255, 0,   255)
#define SFCML_GRAY      sfcml_rgb(128, 128, 128)
#define SFCML_DARKGRAY  sfcml_rgb(64,  64,  64 )
#define SFCML_ORANGE    sfcml_rgb(255, 128, 0  )
#define SFCML_TRANSPARENT sfcml_rgba(0, 0, 0, 0)

/* Melange alpha simple */
sfcml_Color sfcml_blend(sfcml_Color src, sfcml_Color dst);

/* ============================================================
 * Vecteurs et rectangle
 * ============================================================ */
typedef struct { int x, y; }          sfcml_Vec2i;
typedef struct { unsigned w, h; }     sfcml_Vec2u;
typedef struct { int x, y, w, h; }    sfcml_Rect;

static inline sfcml_Rect sfcml_rect(int x, int y, int w, int h) {
    return (sfcml_Rect){ x, y, w, h };
}

/* ============================================================
 * Fenetre (= framebuffer VESA)
 * ============================================================ */
typedef struct {
    uint8_t*  fb;       /* pointeur framebuffer (VESA LFB) */
    uint8_t*  back;     /* backbuffer RAM — double buffering */
    unsigned  width;
    unsigned  height;
    unsigned  pitch;    /* octets par ligne */
    uint8_t   bpp;
    uint8_t*  font;     /* police BIOS 8x8 */
    int       open;
} sfcml_Window;

/* Cree et initialise la fenetre a partir du BOOT_INFO VESA */
sfcml_Window* sfcml_createWindow(const char* title);
void          sfcml_destroyWindow(sfcml_Window* win);
int           sfcml_isOpen(const sfcml_Window* win);
void          sfcml_close(sfcml_Window* win);

/* Efface tout le framebuffer avec une couleur */
void sfcml_clear(sfcml_Window* win, sfcml_Color color);

/* Copie le backbuffer vers le framebuffer en une seule passe rapide */
void sfcml_present(sfcml_Window* win);

/* Infos */
static inline unsigned sfcml_getWidth (const sfcml_Window* win) { return win->width;  }
static inline unsigned sfcml_getHeight(const sfcml_Window* win) { return win->height; }

/* ============================================================
 * Evenements clavier
 * ============================================================ */
typedef enum {
    SFCML_KEY_NONE = 0,
    SFCML_KEY_A, SFCML_KEY_B, SFCML_KEY_C, SFCML_KEY_D, SFCML_KEY_E,
    SFCML_KEY_F, SFCML_KEY_G, SFCML_KEY_H, SFCML_KEY_I, SFCML_KEY_J,
    SFCML_KEY_K, SFCML_KEY_L, SFCML_KEY_M, SFCML_KEY_N, SFCML_KEY_O,
    SFCML_KEY_P, SFCML_KEY_Q, SFCML_KEY_R, SFCML_KEY_S, SFCML_KEY_T,
    SFCML_KEY_U, SFCML_KEY_V, SFCML_KEY_W, SFCML_KEY_X, SFCML_KEY_Y,
    SFCML_KEY_Z,
    SFCML_KEY_0, SFCML_KEY_1, SFCML_KEY_2, SFCML_KEY_3, SFCML_KEY_4,
    SFCML_KEY_5, SFCML_KEY_6, SFCML_KEY_7, SFCML_KEY_8, SFCML_KEY_9,
    SFCML_KEY_ESCAPE,
    SFCML_KEY_RETURN,
    SFCML_KEY_SPACE,
    SFCML_KEY_BACKSPACE,
    SFCML_KEY_TAB,
    SFCML_KEY_UP, SFCML_KEY_DOWN, SFCML_KEY_LEFT, SFCML_KEY_RIGHT,
    SFCML_KEY_F1,  SFCML_KEY_F2,  SFCML_KEY_F3,  SFCML_KEY_F4,
    SFCML_KEY_F5,  SFCML_KEY_F6,  SFCML_KEY_F7,  SFCML_KEY_F8,
    SFCML_KEY_F9,  SFCML_KEY_F10, SFCML_KEY_F11, SFCML_KEY_F12,
    SFCML_KEY_COUNT
} sfcml_KeyCode;

typedef enum {
    SFCML_EVT_NONE = 0,
    SFCML_EVT_CLOSED,
    SFCML_EVT_KEY_PRESSED,
    SFCML_EVT_KEY_RELEASED,
    SFCML_EVT_TEXT,
    SFCML_EVT_MOUSE_MOVED,
    SFCML_EVT_MOUSE_PRESSED,
    SFCML_EVT_MOUSE_RELEASED,
} sfcml_EventType;

typedef struct {
    sfcml_EventType type;
    union {
        struct {
            sfcml_KeyCode code;
            int           shift, ctrl, alt;
        } key;
        struct {
            char ch;
        } text;
        struct {
            int x, y;
            int button; /* 0=gauche, 1=droite */
        } mouse;
    };
} sfcml_Event;

int  sfcml_pollEvent(sfcml_Window* win, sfcml_Event* evt);
void sfcml_waitEvent(sfcml_Window* win, sfcml_Event* evt);
int  sfcml_isKeyPressed(sfcml_KeyCode key);

void sfcml_mouseInit(void);
int  sfcml_getMouseX(void);
int  sfcml_getMouseY(void);
int  sfcml_getMouseButton(int btn);
void sfcml_warpMouse(int x, int y);   /* replace le curseur (mouse-look) */

/* ============================================================
 * Dessin 2D
 * ============================================================ */

/* Pixel */
void sfcml_drawPixel(sfcml_Window* win, int x, int y, sfcml_Color c);

/* Lignes */
void sfcml_drawLine(sfcml_Window* win, int x0, int y0, int x1, int y1, sfcml_Color c);
void sfcml_drawHLine(sfcml_Window* win, int x, int y, int len, sfcml_Color c);
void sfcml_drawVLine(sfcml_Window* win, int x, int y, int len, sfcml_Color c);

/* Rectangles */
void sfcml_drawRect(sfcml_Window* win, sfcml_Rect r, sfcml_Color c);
void sfcml_fillRect(sfcml_Window* win, sfcml_Rect r, sfcml_Color c);

/* Rectangle avec bordure et fond distinct */
void sfcml_drawRectEx(sfcml_Window* win, sfcml_Rect r,
                      sfcml_Color border, sfcml_Color fill);

/* Cercles */
void sfcml_drawCircle(sfcml_Window* win, int cx, int cy, int radius, sfcml_Color c);
void sfcml_fillCircle(sfcml_Window* win, int cx, int cy, int radius, sfcml_Color c);

/* Triangles */
void sfcml_drawTriangle(sfcml_Window* win,
                        int x0, int y0, int x1, int y1, int x2, int y2,
                        sfcml_Color c);
void sfcml_fillTriangle(sfcml_Window* win,
                        int x0, int y0, int x1, int y1, int x2, int y2,
                        sfcml_Color c);

/* ============================================================
 * Texte (police BIOS 8x8)
 * ============================================================ */
void sfcml_drawChar(sfcml_Window* win, char ch, int x, int y,
                    sfcml_Color fg, sfcml_Color bg);
void sfcml_drawText(sfcml_Window* win, const char* text, int x, int y,
                    sfcml_Color fg, sfcml_Color bg);
void sfcml_drawTextf(sfcml_Window* win, int x, int y,
                     sfcml_Color fg, sfcml_Color bg,
                     const char* fmt, ...);

/* ============================================================
 * Image (bitmap en memoire)
 * ============================================================ */
typedef struct {
    unsigned  width;
    unsigned  height;
    uint8_t*  pixels;   /* R G B, 3 octets par pixel */
} sfcml_Image;

sfcml_Image* sfcml_createImage(unsigned w, unsigned h, sfcml_Color fill);
void         sfcml_destroyImage(sfcml_Image* img);
void         sfcml_imageSetPixel(sfcml_Image* img, int x, int y, sfcml_Color c);
sfcml_Color  sfcml_imageGetPixel(const sfcml_Image* img, int x, int y);
void         sfcml_drawImage(sfcml_Window* win, const sfcml_Image* img, int x, int y);
void         sfcml_drawImageScaled(sfcml_Window* win, const sfcml_Image* img,
                                   int x, int y, unsigned w, unsigned h);

/* ============================================================
 * Chronometre
 * ============================================================ */
typedef struct { uint32_t start_ticks; } sfcml_Clock;

void     sfcml_clockStart  (sfcml_Clock* clk);
uint32_t sfcml_clockElapsed(sfcml_Clock* clk);   /* millisecondes */
void     sfcml_clockRestart(sfcml_Clock* clk);

/* ============================================================
 * Initialisation globale
 * ============================================================ */
int  sfcml_init(void);
void sfcml_shutdown(void);

#endif /* _SFCML_H */
