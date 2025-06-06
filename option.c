#include "option.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAIL fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]); exit(EXIT_FAILURE);

const char helpful_message[] = "\
Usage: %s [OPTIONS] FILE\n\
\n\
Options:\n\
  -d            Debug the input file\n\
  -i            Interpret the input file\n\
  -S            Compile only (do not assemble or link)\n\
  -c            Assemble only (do not link)\n\
  -O            Compile with optimizations\n\
  -o FILE       Place the output into FILE\n\
  -m SIZE       Set memory size (default: 30000)\n\
  --help        Display this message and exit\n\
\n\
Operands:\n\
  FILE          Input file to process\n\
\n\
Notes:\n\
  - Exactly one input file must be specified\n\
  - When compiling, memory size is rounded up to the nearest 4K\n\
  - Assembly requires nasm (The Netwide Assembler)\n\
\n\
";

static const struct option long_options[] = {
	{ "help", no_argument, NULL, 0 }, 	
};

static const char * const short_options = "dicSOo:m:";

void parse_options(Options *opts, int argc, char *argv[]) {
	int opt, opt_idx;
	
	opts->flags = 0;
	opts->mem_size = 30000;
	opts->infile = NULL;
	opts->outfile = NULL;

	do {
		while ((opt = getopt_long(argc, argv, short_options, long_options, &opt_idx)) != -1) {
			switch (opt) {
			case 0:
				if (opt_idx == 0) {
					printf(helpful_message, argv[0]);
					exit(EXIT_SUCCESS);
				}
				break;

			case 'd':
				opts->flags |= 1 << DEBUG;
				break;

			case 'i':
				opts->flags |= 1 << INTERPRET;
				break;

			case 'S':
				opts->flags |= 1 << COMPILE_ONLY;
				break;
	
			case 'c':
				opts->flags |= 1 << ASSEMBLE_ONLY;
				break;

			case 'O':
				opts->flags |= 1 << OPTIMIZE;
				break;

			case 'o':
				//This doesn't allow -oname. Space needed
				opts->outfile = argv[optind - 1];
				break;

			case 'm':
				opts->mem_size = atoi(argv[optind - 1]);
				break;

			default:
				FAIL
			}
		}

		if (argc > optind) {
			if (opts->infile == NULL) {
				opts->infile = argv[optind++];
			}
			else {
				fprintf(stderr, "%s: '%s' unexpected. Too many arguments\n", argv[0], argv[++optind]);
				FAIL
			}
		}
	} while (argc > optind);

	if (!opts->infile) {
		fprintf(stderr, "%s: missing input file argument\n", argv[0]);
		FAIL
	}
}
