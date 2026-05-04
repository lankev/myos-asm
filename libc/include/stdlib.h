#ifndef _STDLIB_H
#define _STDLIB_H

#include <stdint.h>

void*  malloc (size_t size);
void   free   (void* ptr);
void*  realloc(void* ptr, size_t new_size);
void*  calloc (size_t nmemb, size_t size);

int    atoi(const char* s);
long   atol(const char* s);
char*  itoa(int val, char* buf, int base);
char*  utoa(unsigned val, char* buf, int base);

int    abs(int x);
int    min(int a, int b);
int    max(int a, int b);

void   exit(int code);
void   abort(void);

#endif
