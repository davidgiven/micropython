#pragma once
#include "common/types.h"

typedef u32 wint_t;
typedef s32 ptrdiff_t;

#define offsetof(st, m) \
    ((size_t)&(((st *)0)->m))
