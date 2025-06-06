#ifndef INTERPRETER
#define INTERPRETER

#include <stddef.h>
#include "option.h"

typedef enum {
	SUCCESS, UNBALANCED_BRACKETS, OUT_OF_BOUNDS
} Error;

Error interpret(const char *code, size_t len, Options *opt);
Error debug(const char *code, size_t len, Options *opt);

#endif
