#pragma once
#include <stdlib.h>
#include <stddef.h>

extern int sprintf(char *restrict str, const char *restrict format, ...);
extern int snprintf(char *str, size_t size, const char *restrict format, ...);
