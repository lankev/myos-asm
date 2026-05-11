#include "string.h"
#include "stdint.h"
#include "stdlib.h"

size_t strlen(const char* s) {
    const char* p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char* d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

char* strcat(char* dst, const char* src) {
    char* d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

char* strncat(char* dst, const char* src, size_t n) {
    char* d = dst;
    while (*d) d++;
    while (n-- && *src) *d++ = *src++;
    *d = '\0';
    return dst;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : NULL;
}

char* strstr(const char* hay, const char* needle) {
    if (!*needle) return (char*)hay;
    size_t nlen = strlen(needle);
    while (*hay) {
        if (!strncmp(hay, needle, nlen)) return (char*)hay;
        hay++;
    }
    return NULL;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint32_t* d4=(uint32_t*)dst; const uint32_t* s4=(const uint32_t*)src;
    while(n>=4){*d4++=*s4++;n-=4;}
    uint8_t* d=(uint8_t*)d4; const uint8_t* s=(const uint8_t*)s4;
    while(n--)*d++=*s++;
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void* memset(void* dst, int val, size_t n) {
    uint8_t v=(uint8_t)val;
    uint32_t v4=(uint32_t)v|((uint32_t)v<<8)|((uint32_t)v<<16)|((uint32_t)v<<24);
    uint32_t* d4=(uint32_t*)dst;
    while(n>=4){*d4++=v4;n-=4;}
    uint8_t* d=(uint8_t*)d4;
    while(n--)*d++=v;
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* p = (const uint8_t*)a;
    const uint8_t* q = (const uint8_t*)b;
    while (n--) {
        if (*p != *q) return *p - *q;
        p++; q++;
    }
    return 0;
}

void* memchr(const void* s, int c, size_t n) {
    const uint8_t* p=(const uint8_t*)s;
    while(n--){if(*p==(uint8_t)c)return(void*)p;p++;}
    return NULL;
}

char* strrchr(const char* s, int c) {
    const char* last=NULL;
    while(*s){if(*s==(char)c)last=s;s++;}
    return (c=='\0')?(char*)s:(char*)last;
}

char* strdup(const char* s) {
    size_t n=strlen(s)+1;
    char* p=(char*)malloc(n);
    if(p)memcpy(p,s,n);
    return p;
}

static char* _tok_ptr=NULL;
char* strtok(char* s, const char* delim) {
    if(s)_tok_ptr=s;
    if(!_tok_ptr)return NULL;
    while(*_tok_ptr&&strchr(delim,*_tok_ptr))_tok_ptr++;
    if(!*_tok_ptr){_tok_ptr=NULL;return NULL;}
    char* start=_tok_ptr;
    while(*_tok_ptr&&!strchr(delim,*_tok_ptr))_tok_ptr++;
    if(*_tok_ptr)*_tok_ptr++='\0';
    else _tok_ptr=NULL;
    return start;
}
