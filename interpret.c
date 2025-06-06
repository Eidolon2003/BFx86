#include "interpret.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h> //min/max
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <unistd.h> //STDIN_FILENO

Error interpret(const char *code, size_t len, Options *opts) {
	Error retval = SUCCESS;
	unsigned char *mem = calloc(opts->mem_size, 1);
	unsigned char *endptr = mem + opts->mem_size;
	unsigned char *memptr = mem;
	if (!mem) {
		fputs("Memory allocation error\n", stderr);
		exit(EXIT_FAILURE);
	}

	size_t open_stack[1024];
	size_t stack_idx = 0;

	ssize_t *close_cache = malloc(len * sizeof(ssize_t));
	if (!close_cache) {
		fputs("Memory allocation error\n", stderr);
		exit(EXIT_FAILURE);
	}
	for (size_t i = 0; i < len; i++) {
		close_cache[i] = -1;
	}

	size_t i = 0;
	while (i < len) {
		//printf("%c", code[i]);
		switch (code[i]) {
			case '+':
				for(; i < len && code[i] == '+'; i++) {
					(*memptr)++;
				}
				break;
			
			case '-':
				for(; i < len && code[i] == '-'; i++) {
					(*memptr)--;
				}
				break;
			
			case '>':
				for (; i < len && code[i] == '>'; i++) {
					memptr++;
				}
				if (memptr >= endptr) {
					retval = OUT_OF_BOUNDS;
					goto ret;
				}
				break;
			
			case '<':
				for (; i < len && code[i] == '<'; i++) {
					memptr--;
				}
				if (memptr < mem) {
					retval = OUT_OF_BOUNDS;
					goto ret;
				}
				break;
				
			case '.':
				putc(*memptr, stdout);
				//Display every individual char, avoid buffering
				fflush(stdout);
				i++;
				break;
			
			case ',':
				*memptr = getc(stdin);
				i++;
				break;
							
			case '[':
				if (*memptr == 0) {
					if (close_cache[i] != -1) {
						i = close_cache[i];
					}
					else {
						size_t open = i;
						int balance_count = 0;
						for (; i < len; i++) {
							if (code[i] == '[') {
								balance_count++;
							}
							
							if (code[i] == ']' && --balance_count == 0) {
								close_cache[open] = i;
								break;
							}
						}

						if (i == len) {
							retval = UNBALANCED_BRACKETS;
							goto ret;
						}
					}
				}
				else {
					open_stack[stack_idx++] = i;
				}
				i++;
				break;
			
			case ']': {
				if (stack_idx == 0) {
					retval = UNBALANCED_BRACKETS;
					goto ret;
				}

				size_t open = open_stack[stack_idx - 1];
				close_cache[open] = i;

				if (*memptr != 0) {
					i = open;
				}
				else {
					stack_idx--;
				}
				i++;
				break;
			}

			default:
				i++;
				break;
		}
	}

ret:
	free(mem);
	free(close_cache);
	return retval;
}

Error debug(const char *rawcode, size_t rawlen, Options *opts) {
	for (int i = 0; i < 100; i++) {
		putchar('\n');
	}

	Error retval = SUCCESS;
	
	unsigned char *mem = calloc(opts->mem_size, 1);
	unsigned char *endptr = mem + opts->mem_size;
	unsigned char *memptr = mem;
	if (!mem) {
		fputs("Memory allocation error\n", stderr);
		exit(EXIT_FAILURE);
	}

	char *code = malloc(rawlen);
	if (!code) {
		fputs("Memory allocation error\n", stderr);
		exit(EXIT_FAILURE);
	}
	size_t len = 0;
	for (size_t i = 0; i < rawlen; i++) {
		if (strchr("+-<>.,[]#", rawcode[i]) != NULL) {
			code[len++] = rawcode[i];
		}
	}
	
	char *output_buffer = malloc(256);
	size_t output_size = 256;
	size_t output_idx = 0;
	if (!output_buffer) {
		fputs("Memory allocation error\n", stderr);
		exit(EXIT_FAILURE);
	} 

	size_t open_stack[1024];
	size_t stack_idx = 0;

	ssize_t *close_cache = malloc(len * sizeof(ssize_t));
	if (!close_cache) {
		fputs("Memory allocation error\n", stderr);
		exit(EXIT_FAILURE);
	}
	for (size_t i = 0; i < len; i++) {
		close_cache[i] = -1;
	}

	//configure settings so getchar doesn't require pressing enter
	//will reset after
	static struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	int running = 0;
	for (size_t i = 0; i < len; i++) {
		printf("Output:\n%.*s\n\n", (unsigned)output_idx, output_buffer);

		size_t code_left = MAX(0, (int)i - 12);
		size_t code_arrow = i - code_left;
		size_t code_right = MIN(len, code_left + 25);
		printf("Code [%lu]:\n%.*s\n", i, (unsigned)(code_right - code_left), code + code_left);
		for (size_t j = 0; j < code_arrow; j++) {
			putchar(' ');
		}
		puts("^\n");

		unsigned char *mem_left = MAX(mem, memptr - 4);
		size_t mem_arrow = memptr - mem_left;
		unsigned char *mem_right = MIN(endptr, mem_left + 9);
		printf("Memory [%ld]:\n", memptr - mem);
		for (unsigned char *p = mem_left; p != mem_right; p++) {
			printf("%03d ", *p);
		}
		putchar('\n');
		for (size_t j = 0; j < mem_arrow; j++) {
			printf("    ");
		}
		puts(" ^\n");

		printf("s - step, c - continue, x - exit\n\n\n\n");
		char input;
		do {
			input = getchar();
		} while (strchr("sScCxX", input) == NULL);
		switch (input) {
		case 's':
		case 'S':
			running = 0;
			break;

		case 'c':
		case 'C':
			running = 1;
			break;

		case 'x':
		case 'X':
			goto ret;
		}

		for (; i < len; i++) {
			switch (code[i]) {
			case '+':
				(*memptr)++;
				break;

			case '-':
				(*memptr)--;
				break;

			case '>':
				memptr++;
				if (memptr >= endptr) {
					retval = OUT_OF_BOUNDS;
					goto ret;
				}
				break;

			case '<':
				memptr--;
				if (memptr < mem) {
					retval = OUT_OF_BOUNDS;
					goto ret;
				}
				break;

			case '.':
				if (output_idx == output_size) {
					output_size *= 2;
					void *t = realloc(output_buffer, output_size);
					output_buffer = t;
				}
				output_buffer[output_idx++] = *memptr;
				putchar(*memptr);
				fflush(stdout);
				break;

			case ',':
				putchar('?');
				*memptr = getc(stdin);
				break;

			case '[':
				if (*memptr == 0) {
					if (close_cache[i] != -1) {
						i = close_cache[i];
					}
					else {
						size_t open = i;
						int balance_count = 0;
						for (; i < len; i++) {
							if (code[i] == '[') {
								balance_count++;
							}
											
							if (code[i] == ']' && --balance_count == 0) {
								close_cache[open] = i;
								break;
							}
						}
				
						if (i == len) {
							retval = UNBALANCED_BRACKETS;
							goto ret;
						}
					}
				}
				else {
					open_stack[stack_idx++] = i;
				}
				break;

			case ']':
				if (stack_idx == 0) {
					retval = UNBALANCED_BRACKETS;
					goto ret;
				}

				size_t open = open_stack[stack_idx - 1];
				close_cache[open] = i;

				if (*memptr != 0) {
					i = open;
				}
				else {
					stack_idx--;
				}
				break;

			case '#':
				running = 0;
				break;
			}

			if (!running) break;
		}
	}
	
ret:
	free(close_cache);
	free(output_buffer);
	free(code);
	free(mem);
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return retval;
}
