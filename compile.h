#ifndef COMPILER
#define COMPILER

#include <stddef.h>
#include "option.h"

void compile(const char *code, size_t len, Options *opt);

#endif
