#ifndef _STDDEF_H
#define _STDDEF_H

#include "stdint.h"

#define offsetof(type, member) ((size_t)&((type*)0)->member)

#endif
