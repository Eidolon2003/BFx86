#ifndef OPTION
#define OPTION

typedef enum {
	INTERPRET, COMPILE_ONLY, ASSEMBLE_ONLY, OPTIMIZE, DEBUG
} OptID;

typedef struct {
	unsigned flags;
	unsigned mem_size;
	char *infile;
	char *outfile;
} Options;

void parse_options(Options *opt, int argc, char *argv[]);

static inline int is_enabled(Options *opt, OptID i) { 
	return opt->flags & (1 << i); 
}

#endif
