#ifndef _STDIO_H
#define _STDIO_H

#include "stdint.h"

/* Fourni par l'OS ou l'application : sortie d'un seul caractere */
extern void _putchar(char c);

int putchar(int c);
int puts   (const char* s);
int printf (const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t n, const char* fmt, ...);

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_arg(v, t)   __builtin_va_arg(v, t)
#define va_end(v)      __builtin_va_end(v)

int vsnprintf(char* buf, size_t n, const char* fmt, va_list ap);

#endif
