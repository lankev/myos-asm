#ifndef _STDINT_H
#define _STDINT_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint32_t  size_t;
typedef int32_t   ssize_t;
typedef uint32_t  uintptr_t;
typedef int32_t   intptr_t;

#define UINT8_MAX   0xFF
#define UINT16_MAX  0xFFFF
#define UINT32_MAX  0xFFFFFFFFU
#define INT8_MAX    0x7F
#define INT16_MAX   0x7FFF
#define INT32_MAX   0x7FFFFFFF
#define INT8_MIN    (-0x80)
#define INT16_MIN   (-0x8000)
#define INT32_MIN   (-0x80000000)

#define NULL ((void*)0)

#endif
