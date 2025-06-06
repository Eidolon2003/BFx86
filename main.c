#include "interpret.h"
#include "option.h"
#include "compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t get_len(FILE *file) {
	fseek(file, 0, SEEK_END);
	size_t len = ftell(file);
	rewind(file);
	return len;
}

int main(int argc, char **argv) {
	Options opts;
	parse_options(&opts, argc, argv);

	if (opts.infile && strlen(opts.infile) >= 256) {
		fputs("Error: input filename too long\n", stderr);
		return EXIT_FAILURE;
	}
	if (opts.outfile && strlen(opts.outfile) >= 256) {
		fputs("Error: output filename too long\n", stderr);
		return EXIT_FAILURE;
	}

	FILE *infile = fopen(opts.infile, "r");
	if (!infile) {
		fprintf(stderr, "Failed to open \"%s\"\n", opts.infile);
		return EXIT_FAILURE;
	}

	size_t len = get_len(infile);

	char *code = malloc(len);
	if (!code) {
		fputs("Memory allocation error\n", stderr);
		fclose(infile);
		return EXIT_FAILURE;
	}

	fread(code, sizeof(char), len, infile);
	fclose(infile);

	Error e = SUCCESS;
	if (is_enabled(&opts, DEBUG)) {
		e = debug(code, len, &opts);
	}
	else {
		if (is_enabled(&opts, INTERPRET)) {
			e = interpret(code, len, &opts);
		}
		else {
			compile(code, len, &opts);
		}
	}

	switch (e) {
	case UNBALANCED_BRACKETS:
		fputs("Error: unbalanced brackets\n", stderr);
		break;

	case OUT_OF_BOUNDS:
		fputs("Error: memory out of bounds\n", stderr);
		break;

	case SUCCESS:
		break;
	}
	
	free(code);
	return EXIT_SUCCESS;
}
