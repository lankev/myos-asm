#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"

int putchar(int c) {
    _putchar((char)c);
    return c;
}

int puts(const char* s) {
    while (*s) _putchar(*s++);
    _putchar('\n');
    return 0;
}

/* ---- Formatage interne ---- */

typedef struct {
    char*  buf;     /* NULL = sortie directe via _putchar */
    size_t pos;
    size_t cap;
} _fmt_ctx;

static void _emit(_fmt_ctx* ctx, char c) {
    if (ctx->buf) {
        if (ctx->pos + 1 < ctx->cap)
            ctx->buf[ctx->pos] = c;
        ctx->pos++;
    } else {
        _putchar(c);
        ctx->pos++;
    }
}

static void _emit_str(_fmt_ctx* ctx, const char* s, int width, int left) {
    int len = (int)strlen(s);
    int pad = width > len ? width - len : 0;
    if (!left) while (pad--) _emit(ctx, ' ');
    while (*s) _emit(ctx, *s++);
    if (left)  while (pad--) _emit(ctx, ' ');
}

static void _emit_int(_fmt_ctx* ctx, long val, int base, int upper,
                       int width, int pad_zero, int left, int sign) {
    char buf[34];
    char tmp[34];
    int  i = 0, neg = 0;

    if (val < 0 && base == 10) { neg = 1; val = -val; }
    unsigned long u = (unsigned long)val;
    if (!u) { tmp[i++] = '0'; }
    else while (u) {
        int d = u % base;
        tmp[i++] = (d < 10) ? ('0' + d) : (upper ? 'A' : 'a') + d - 10;
        u /= base;
    }
    if (neg)  tmp[i++] = '-';
    else if (sign == '+') tmp[i++] = '+';

    int len = i;
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';

    int pad = width > len ? width - len : 0;
    char pc = pad_zero ? '0' : ' ';
    if (!left) while (pad--) _emit(ctx, pc);
    for (int k = 0; k < len; k++) _emit(ctx, buf[k]);
    if (left)  while (pad--) _emit(ctx, ' ');
}

static int _vfmt(_fmt_ctx* ctx, const char* fmt, va_list ap) {
    while (*fmt) {
        if (*fmt != '%') { _emit(ctx, *fmt++); continue; }
        fmt++;

        /* Flags */
        int left = 0, plus = 0, pad_zero = 0;
        while (*fmt == '-' || *fmt == '+' || *fmt == '0') {
            if (*fmt == '-') left = 1;
            if (*fmt == '+') plus = 1;
            if (*fmt == '0') pad_zero = 1;
            fmt++;
        }
        /* Largeur */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; }
        char spec = *fmt++;
        switch (spec) {
        case 'd': case 'i':
            _emit_int(ctx, is_long ? va_arg(ap, long) : (long)va_arg(ap, int), 10, 0, width, pad_zero, left, plus ? '+' : 0);
            break;
        case 'u':
            _emit_int(ctx, is_long ? (long)va_arg(ap, unsigned long) : (long)(unsigned)va_arg(ap, unsigned), 10, 0, width, pad_zero, left, 0);
            break;
        case 'x':
            _emit_int(ctx, is_long ? (long)va_arg(ap, unsigned long) : (long)(unsigned)va_arg(ap, unsigned), 16, 0, width, pad_zero, left, 0);
            break;
        case 'X':
            _emit_int(ctx, is_long ? (long)va_arg(ap, unsigned long) : (long)(unsigned)va_arg(ap, unsigned), 16, 1, width, pad_zero, left, 0);
            break;
        case 'o':
            _emit_int(ctx, (long)(unsigned)va_arg(ap, unsigned), 8, 0, width, pad_zero, left, 0);
            break;
        case 'p': {
            _emit(ctx, '0'); _emit(ctx, 'x');
            _emit_int(ctx, (long)(uintptr_t)va_arg(ap, void*), 16, 0, 8, 1, 0, 0);
            break;
        }
        case 'c':
            _emit(ctx, (char)va_arg(ap, int));
            break;
        case 's': {
            const char* s = va_arg(ap, const char*);
            _emit_str(ctx, s ? s : "(null)", width, left);
            break;
        }
        case '%':
            _emit(ctx, '%');
            break;
        default:
            _emit(ctx, '%'); _emit(ctx, spec);
            break;
        }
    }
    if (ctx->buf && ctx->pos < ctx->cap)
        ctx->buf[ctx->pos] = '\0';
    return (int)ctx->pos;
}

int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _fmt_ctx ctx = { NULL, 0, 0 };
    int r = _vfmt(&ctx, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _fmt_ctx ctx = { buf, 0, (size_t)-1 };
    int r = _vfmt(&ctx, fmt, ap);
    va_end(ap);
    return r;
}

int snprintf(char* buf, size_t n, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _fmt_ctx ctx = { buf, 0, n };
    int r = _vfmt(&ctx, fmt, ap);
    va_end(ap);
    return r;
}

int vsnprintf(char* buf, size_t n, const char* fmt, va_list ap) {
    _fmt_ctx ctx = { buf, 0, n };
    return _vfmt(&ctx, fmt, ap);
}
