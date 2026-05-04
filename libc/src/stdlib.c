#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ========== Heap (free-list allocateur) ==========
 * Heap place a 4 Mo, taille 4 Mo.
 * Chaque bloc = header (block_t) + donnees utilisateur.
 * Fusion des blocs libres adjacents a chaque free().
 */
#define HEAP_BASE  0x00400000U
#define HEAP_SIZE  (4U * 1024U * 1024U)
#define ALIGN      8U

typedef struct block {
    uint32_t      size;   /* taille des donnees (sans le header) */
    uint8_t       used;
    struct block* next;
} block_t;

static block_t* _heap = NULL;

static void _heap_init(void) {
    _heap = (block_t*)HEAP_BASE;
    _heap->size = HEAP_SIZE - sizeof(block_t);
    _heap->used = 0;
    _heap->next = NULL;
}

static size_t _align(size_t n) {
    return (n + ALIGN - 1) & ~(ALIGN - 1);
}

void* malloc(size_t size) {
    if (!_heap) _heap_init();
    if (!size) return NULL;
    size = _align(size);

    for (block_t* b = _heap; b; b = b->next) {
        if (b->used || b->size < size) continue;

        /* Divise le bloc si assez grand */
        if (b->size >= size + sizeof(block_t) + ALIGN) {
            block_t* split = (block_t*)((uint8_t*)(b + 1) + size);
            split->size = b->size - size - sizeof(block_t);
            split->used = 0;
            split->next = b->next;
            b->next = split;
            b->size = size;
        }
        b->used = 1;
        return (void*)(b + 1);
    }
    return NULL;
}

void free(void* ptr) {
    if (!ptr) return;
    block_t* b = (block_t*)ptr - 1;
    b->used = 0;
    /* Fusionne avec les blocs libres suivants */
    while (b->next && !b->next->used) {
        b->size += sizeof(block_t) + b->next->size;
        b->next  = b->next->next;
    }
}

void* realloc(void* ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (!new_size) { free(ptr); return NULL; }
    block_t* b = (block_t*)ptr - 1;
    new_size = _align(new_size);
    if (b->size >= new_size) return ptr;
    void* p = malloc(new_size);
    if (p) {
        memcpy(p, ptr, b->size < new_size ? b->size : new_size);
        free(ptr);
    }
    return p;
}

void* calloc(size_t nmemb, size_t size) {
    void* p = malloc(nmemb * size);
    if (p) memset(p, 0, nmemb * size);
    return p;
}

/* ========== Conversions ========== */

int atoi(const char* s) {
    int n = 0, sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return n * sign;
}

long atol(const char* s) {
    return (long)atoi(s);
}

char* itoa(int val, char* buf, int base) {
    char tmp[34];
    int  i = 0, neg = 0;
    if (!val) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    if (val < 0 && base == 10) { neg = 1; val = -val; }
    unsigned u = (unsigned)val;
    while (u) {
        int d = u % base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        u /= base;
    }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

char* utoa(unsigned val, char* buf, int base) {
    char tmp[34];
    int i = 0;
    if (!val) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    while (val) {
        int d = val % base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        val /= base;
    }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

/* ========== Utilitaires ========== */

int abs(int x) { return x < 0 ? -x : x; }
int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }

void exit(int code) {
    (void)code;
    __asm__ volatile("cli; hlt");
    for (;;);
}

void abort(void) { exit(1); }
