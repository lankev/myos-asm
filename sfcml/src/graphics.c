#include "sfcml.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "stdint.h"

/* ============================================================
 * Lignes
 * ============================================================ */

void sfcml_drawHLine(sfcml_Window* win, int x, int y, int len, sfcml_Color c) {
    if(!win->back||(unsigned)y>=win->height)return;
    if(x<0){len+=x;x=0;}
    if(x+(int)len>(int)win->width)len=(int)win->width-x;
    if(len<=0)return;
    uint8_t*p=win->back+(uint32_t)y*win->pitch+(uint32_t)x*3;
    uint8_t cb=c.b,cg=c.g,cr=c.r;
    for(int i=0;i<len;i++){*p++=cb;*p++=cg;*p++=cr;}
}

void sfcml_drawVLine(sfcml_Window* win, int x, int y, int len, sfcml_Color c) {
    for (int i = 0; i < len; i++) sfcml_drawPixel(win, x, y + i, c);
}

/* Bresenham */
void sfcml_drawLine(sfcml_Window* win, int x0, int y0, int x1, int y1, sfcml_Color c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        sfcml_drawPixel(win, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ============================================================
 * Rectangles
 * ============================================================ */

void sfcml_fillRect(sfcml_Window* win, sfcml_Rect r, sfcml_Color c) {
    int x0=r.x,y0=r.y,x1=r.x+r.w,y1=r.y+r.h;
    if(x0<0)x0=0;if(y0<0)y0=0;
    if(x1>(int)win->width)x1=(int)win->width;
    if(y1>(int)win->height)y1=(int)win->height;
    if(x0>=x1||y0>=y1||!win->back)return;
    uint8_t cb=c.b,cg=c.g,cr=c.r;
    int w=x1-x0;
    for(int y=y0;y<y1;y++){
        uint8_t*p=win->back+(uint32_t)y*win->pitch+(uint32_t)x0*3;
        for(int x=0;x<w;x++){*p++=cb;*p++=cg;*p++=cr;}
    }
}

void sfcml_drawRect(sfcml_Window* win, sfcml_Rect r, sfcml_Color c) {
    sfcml_drawHLine(win, r.x,           r.y,           r.w, c);
    sfcml_drawHLine(win, r.x,           r.y + r.h - 1, r.w, c);
    sfcml_drawVLine(win, r.x,           r.y,           r.h, c);
    sfcml_drawVLine(win, r.x + r.w - 1, r.y,           r.h, c);
}

void sfcml_drawRectEx(sfcml_Window* win, sfcml_Rect r,
                      sfcml_Color border, sfcml_Color fill) {
    sfcml_fillRect(win, r, fill);
    sfcml_drawRect(win, r, border);
}

/* ============================================================
 * Cercles (algorithme du point median)
 * ============================================================ */

static void _circle_pts(sfcml_Window* win, int cx, int cy,
                         int x, int y, sfcml_Color c) {
    sfcml_drawPixel(win, cx+x, cy+y, c);
    sfcml_drawPixel(win, cx-x, cy+y, c);
    sfcml_drawPixel(win, cx+x, cy-y, c);
    sfcml_drawPixel(win, cx-x, cy-y, c);
    sfcml_drawPixel(win, cx+y, cy+x, c);
    sfcml_drawPixel(win, cx-y, cy+x, c);
    sfcml_drawPixel(win, cx+y, cy-x, c);
    sfcml_drawPixel(win, cx-y, cy-x, c);
}

void sfcml_drawCircle(sfcml_Window* win, int cx, int cy, int r, sfcml_Color c) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
        _circle_pts(win, cx, cy, x, y, c);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

void sfcml_fillCircle(sfcml_Window* win, int cx, int cy, int r, sfcml_Color c) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
        sfcml_drawHLine(win, cx - x, cy + y, 2 * x + 1, c);
        sfcml_drawHLine(win, cx - x, cy - y, 2 * x + 1, c);
        sfcml_drawHLine(win, cx - y, cy + x, 2 * y + 1, c);
        sfcml_drawHLine(win, cx - y, cy - x, 2 * y + 1, c);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

/* ============================================================
 * Triangles
 * ============================================================ */

void sfcml_drawTriangle(sfcml_Window* win,
                        int x0, int y0, int x1, int y1, int x2, int y2,
                        sfcml_Color c) {
    sfcml_drawLine(win, x0, y0, x1, y1, c);
    sfcml_drawLine(win, x1, y1, x2, y2, c);
    sfcml_drawLine(win, x2, y2, x0, y0, c);
}

/* Remplissage par scanlines */
static void _swap_i(int* a, int* b) { int t = *a; *a = *b; *b = t; }

void sfcml_fillTriangle(sfcml_Window* win,
                        int x0, int y0, int x1, int y1, int x2, int y2,
                        sfcml_Color c) {
    /* Tri des sommets par y croissant */
    if (y0 > y1) { _swap_i(&y0,&y1); _swap_i(&x0,&x1); }
    if (y0 > y2) { _swap_i(&y0,&y2); _swap_i(&x0,&x2); }
    if (y1 > y2) { _swap_i(&y1,&y2); _swap_i(&x1,&x2); }

    int total_h = y2 - y0;
    for (int i = 0; i <= total_h; i++) {
        int second_half = (i > y1 - y0) || (y1 == y0);
        int seg_h = second_half ? (y2 - y1) : (y1 - y0);
        if (!seg_h) seg_h = 1;
        float alpha = (float)i / total_h;
        float beta  = (float)(i - (second_half ? y1 - y0 : 0)) / seg_h;
        int ax = x0 + (int)((x2 - x0) * alpha);
        int bx = second_half ? x1 + (int)((x2 - x1) * beta)
                             : x0 + (int)((x1 - x0) * beta);
        if (ax > bx) _swap_i(&ax, &bx);
        for (int x = ax; x <= bx; x++)
            sfcml_drawPixel(win, x, y0 + i, c);
    }
}

/* ============================================================
 * Texte (police BIOS 8x8)
 * ============================================================ */

void sfcml_drawChar(sfcml_Window* win, char ch, int x, int y,
                    sfcml_Color fg, sfcml_Color bg) {
    if (!win->font) return;
    const uint8_t* glyph = win->font + (uint8_t)ch * 8;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            sfcml_Color c = (bits & (1 << col)) ? fg : bg;
            sfcml_drawPixel(win, x + col, y + row, c);
        }
    }
}

void sfcml_drawText(sfcml_Window* win, const char* text, int x, int y,
                    sfcml_Color fg, sfcml_Color bg) {
    int cx = x;
    while (*text) {
        if (*text == '\n') { cx = x; y += 8; }
        else { sfcml_drawChar(win, *text, cx, y, fg, bg); cx += 8; }
        text++;
    }
}

void sfcml_drawTextf(sfcml_Window* win, int x, int y,
                     sfcml_Color fg, sfcml_Color bg,
                     const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    sfcml_drawText(win, buf, x, y, fg, bg);
}

/* ============================================================
 * Images (bitmap)
 * ============================================================ */

sfcml_Image* sfcml_createImage(unsigned w, unsigned h, sfcml_Color fill) {
    sfcml_Image* img = (sfcml_Image*)malloc(sizeof(sfcml_Image));
    if (!img) return NULL;
    img->width  = w;
    img->height = h;
    img->pixels = (uint8_t*)malloc(w * h * 3);
    if (!img->pixels) { free(img); return NULL; }
    for (unsigned i = 0; i < w * h; i++) {
        img->pixels[i*3+0] = fill.r;
        img->pixels[i*3+1] = fill.g;
        img->pixels[i*3+2] = fill.b;
    }
    return img;
}

void sfcml_destroyImage(sfcml_Image* img) {
    if (!img) return;
    free(img->pixels);
    free(img);
}

void sfcml_imageSetPixel(sfcml_Image* img, int x, int y, sfcml_Color c) {
    if (!img || (unsigned)x >= img->width || (unsigned)y >= img->height) return;
    uint8_t* p = img->pixels + (y * img->width + x) * 3;
    p[0] = c.r; p[1] = c.g; p[2] = c.b;
}

sfcml_Color sfcml_imageGetPixel(const sfcml_Image* img, int x, int y) {
    if (!img || (unsigned)x >= img->width || (unsigned)y >= img->height)
        return SFCML_BLACK;
    const uint8_t* p = img->pixels + (y * img->width + x) * 3;
    return sfcml_rgb(p[0], p[1], p[2]);
}

void sfcml_drawImage(sfcml_Window* win, const sfcml_Image* img, int ox, int oy) {
    for (unsigned y = 0; y < img->height; y++)
        for (unsigned x = 0; x < img->width; x++)
            sfcml_drawPixel(win, ox + (int)x, oy + (int)y,
                            sfcml_imageGetPixel(img, (int)x, (int)y));
}

void sfcml_drawImageScaled(sfcml_Window* win, const sfcml_Image* img,
                            int ox, int oy, unsigned dw, unsigned dh) {
    for (unsigned dy = 0; dy < dh; dy++) {
        int sy = (int)(dy * img->height / dh);
        for (unsigned dx = 0; dx < dw; dx++) {
            int sx = (int)(dx * img->width / dw);
            sfcml_drawPixel(win, ox + (int)dx, oy + (int)dy,
                            sfcml_imageGetPixel(img, sx, sy));
        }
    }
}
