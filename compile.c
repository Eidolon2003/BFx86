#include "compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char header1[] = "\
global %s\n\
\n\
%%define sys_read 0\n\
%%define sys_write 1\n\
%%define sys_mmap 9\n\
%%define sys_munmap 11\n\
%%define sys_exit 60\n\
\n\
%%define stdin 0\n\
%%define stdout 1\n\
\n\
;see /usr/include/x86_64-linux-gnu/bits/mman-linux.h\n\
%%define PROT_READ 1\n\
%%define PROT_WRITE 2\n\
%%define MAP_PRIVATE 2\n\
%%define MAP_ANONYMOUS 32\n\
\n\
%%define ARR rbx\n\
%%define PTR rsi\n\
\n\
section .text\n\
%s:\n\
\tpush\trbx\n\
";
static const char header2[] = "\
\tmov\t\trax, sys_mmap\n\
\tmov\t\trdi, 0\n\
\tmov\t\trsi, %u\n\
\tmov\t\trdx, PROT_READ | PROT_WRITE\n\
\tmov\t\tr10, MAP_PRIVATE | MAP_ANONYMOUS\n\
\tmov\t\tr8, -1\n\
\tmov\t\tr9, 0\n\
\tsyscall\n\
\tmov\t\tARR, rax\n\
\tmov\t\tPTR, rax\n\
\tmov\t\trdx, 1\n\
";

static const char footer_nf[] = "\
\tmov\t\trax, sys_munmap\n\
\tmov\t\trdi, ARR\n\
\tmov\t\trsi, %u\n\
\tsyscall\n\
\tpop\t\trbx\n\
\tmov\t\trax, sys_exit\n\
\tmov\t\trdi, 0\n\
\tsyscall\n\
";

static const char footer_f[] = "\
\tmov\t\tr10b, byte [ARR]\n\
\tmov\t\trax, sys_munmap\n\
\tmov\t\trdi, ARR\n\
\tmov\t\trsi, %u\n\
\tsyscall\n\
\tpop\t\trbx\n\
\tmov\t\tal, r10b\n\
\tret\n\
";

//+-><,.[]
static const char plus[] = "\tinc\t\tbyte [PTR]\n";
static const char minus[] = "\tdec\t\tbyte [PTR]\n";
static const char right[] = "\tinc\t\tPTR\n";
static const char left[] = "\tdec\t\tPTR\n";
static const char comma[] = "\
\tmov\t\trax, sys_read\n\
\tmov\t\trdi, stdin\n\
\tsyscall\n\
";
static const char dot[] = "\
\tmov\t\trax, sys_write\n\
\tmov\t\trdi, stdout\n\
\tsyscall\n\
";
static const char open[] = "\
\ttest\tbyte [PTR], 255\n\
\tjz\t\tclose%u\n\
open%u:\n\
";
static const char close[] = "\
\ttest\tbyte [PTR], 255\n\
\tjnz\t\topen%u\n\
close%u:\n\
";

static const char *push_args[] = {
	"\tpush\trdi\n",
	"\tpush\trsi\n",
	"\tpush\trdx\n",
	"\tpush\trcx\n",
	"\tpush\tr8\n",
	"\tpush\tr9\n",	
};

static const char *pop_args[] = {
	"\tpop\t\tr10\n\tmov\t\tbyte [ARR + 0], r10b\n",
	"\tpop\t\tr10\n\tmov\t\tbyte [ARR + 1], r10b\n",
	"\tpop\t\tr10\n\tmov\t\tbyte [ARR + 2], r10b\n",
	"\tpop\t\tr10\n\tmov\t\tbyte [ARR + 3], r10b\n",
	"\tpop\t\tr10\n\tmov\t\tbyte [ARR + 4], r10b\n",
	"\tpop\t\tr10\n\tmov\t\tbyte [ARR + 5], r10b\n",
};

static char outbuf[256];
static char inbuf[256];
static char *outfile_name;
static char *infile_name;

static inline void compile_unoptimized(const char *code, size_t len, Options *opt) {
	//Round mem_size up to nearest page
	unsigned mem_size = ((opt->mem_size + 4095) >> 12) << 12;

	//Check for function
	int is_function = 0;
	char name[256] = "_start";
	int args = 0;
	if (strncmp(code, "%function", 9) == 0) {
		is_function = 1;
		sscanf(code, "%%function %s %u", name, &args);
	}

	if (args > 6) {
		fputs("Error: functions can take a maximum of six arguments", stderr);
		exit(EXIT_FAILURE);
	}

	FILE *outfile = fopen(outfile_name, "wt");
	fprintf(outfile, header1, name, name);
	if (is_function) {
		for (int i = 0; i < args; i++) {
			fputs(push_args[i], outfile);
		}
	}
	fprintf(outfile, header2, mem_size);
	if (is_function) {
		for (int i = 0; i < args; i++) {
			fputs(pop_args[i], outfile);
		}
	}

	unsigned bracket_stack[1024];
	unsigned stack_idx = 0;
	unsigned bracket_count = 0;

	for (size_t i = 0; i < len; i++) {
		switch (code[i]) {
		case '+':
			fprintf(outfile, plus);
			break;

		case '-':
			fprintf(outfile, minus);
			break;

		case '>':
			fprintf(outfile, right);
			break;

		case '<':
			fprintf(outfile, left);
			break;

		case ',':
			fprintf(outfile, comma);
			break;

		case '.':
			fprintf(outfile, dot);
			break;

		case '[':
			fprintf(outfile, open, bracket_count, bracket_count);
			bracket_stack[stack_idx++] = bracket_count++;
			break;

		case ']': {
			if (stack_idx == 0) {
				fputs("Error: unbalanced brackets\n", stderr);
				exit(EXIT_FAILURE);
			}
			unsigned x = bracket_stack[--stack_idx];
			fprintf(outfile, close, x, x);
			break;
		}

		default:
			break;
		}
	}

	if (is_function) {
		fprintf(outfile, footer_f, mem_size);	
	}
	else {
		fprintf(outfile, footer_nf, mem_size);	
	}
	fclose(outfile);
	return;
}

static const char add_offset[] = "\tadd\t\tbyte [PTR + %d], %d\n";
static const char sub_offset[] = "\tsub\t\tbyte [PTR + %d], %d\n";
static const char add_ptr[] = "\tadd\t\tPTR, %d\n";
static const char comma_short[] = "\
\tmov\t\trax, sys_read\n\
\tsyscall\n\
";
//sys_write = 1,
//but coincidentally rax should already equal 1 from reading/writing 1 byte last syscall!
static const char dot_short[] = "\
\tsyscall\n\
";
static const char open_short[] = "\
\tjz\t\tclose%u\n\
open%u:\n\
";
static const char close_short[] = "\
\tjnz\t\topen%u\n\
close%u:\n\
";
static const char zero_offset[] = "\tmov\t\tbyte [PTR + %d], 0\n";

static inline size_t tokenize(char *tokens, const char *code, size_t len) {
	size_t token_idx = 0;
	size_t i = 0;
	while (i < len) {
		char count = 0;
		switch (code[i]) {
		case '+':
			tokens[token_idx++] = '+';
			for (; code[i] == '+'; i++) {
				count++;
			}
			tokens[token_idx++] = count;
			break;

		case '-':
			tokens[token_idx++] = '-';
			for (; code[i] == '-'; i++) {
				count++;
			}
			tokens[token_idx++] = count;
			break;

		case '>':
			tokens[token_idx++] = '>';
			for (; code[i] == '>'; i++) {
				count++;
			}
			tokens[token_idx++] = count;
			break;

		case '<':
			tokens[token_idx++] = '<';
			for (; code[i] == '<'; i++) {
				count++;
			}
			tokens[token_idx++] = count;
			break;

		case ',':
			tokens[token_idx++] = ',';
			i++;
			break;

		case '.':
			tokens[token_idx++] = '.';
			i++;
			break;

		case '[':
			if (strncmp(code + i, "[-]", 3) == 0) {
				tokens[token_idx++] = '0';
				i += 3;
			}
			else {
				tokens[token_idx++] = '[';
				i++;
			}
			break;

		case ']':
			tokens[token_idx++] = ']';
			i++;
			break;

		default:
			i++;
			break;
		}
	}
	return token_idx;
}

static inline void compile_optimized(const char *code, size_t len, Options *opt) {
	//Round mem_size up to nearest page
	unsigned mem_size = ((opt->mem_size + 4095) >> 12) << 12;

	//Check for function
	int is_function = 0;
	char name[256] = "_start";
	int args = 0;
	if (strncmp(code, "%function", 9) == 0) {
		is_function = 1;
		sscanf(code, "%%function %s %u", name, &args);
	}

	if (args > 6) {
		fputs("Error: functions can take a maximum of six arguments", stderr);
		exit(EXIT_FAILURE);
	}

	//Tokenization
	char *tokens = malloc(len * 2);
	size_t token_len = tokenize(tokens, code, len);

	FILE *outfile = fopen(outfile_name, "wt");
	fprintf(outfile, header1, name, name);
	if (is_function) {
		for (int i = 0; i < args; i++) {
			fputs(push_args[i], outfile);
		}
	}
	fprintf(outfile, header2, mem_size);
	if (is_function) {
		for (int i = 0; i < args; i++) {
			fputs(pop_args[i], outfile);
		}
	}

	unsigned bracket_stack[1024];
	unsigned stack_idx = 0;
	unsigned bracket_count = 0;

	char prevIO = '\0';
	int zf_set = 0;

	for (size_t i = 0; i < token_len; i++) {
		switch (tokens[i]) {
		case '+':
		case '-':
		case '>':
		case '<':
		case '0': {
			int ptr_final = 0;
			for (size_t j = i; strchr(",.[]", tokens[j]) == NULL; j++) {
				if (tokens[j] == '>') ptr_final += tokens[++j];
				else if (tokens[j] == '<') ptr_final -= tokens[++j];
				else if (tokens[j] != '0') j++;
			}

			if (ptr_final != 0) {
				fprintf(outfile, add_ptr, ptr_final);
			}
			
			int zero_add = 0;
			int ptr_current = 0;
			for (; strchr(",.[]", tokens[i]) == NULL; i++) {			
				switch (tokens[i]) {
				case '+':
					if (ptr_current == ptr_final) {
						zero_add += tokens[++i];
					}
					else {
						fprintf(outfile, add_offset, ptr_current - ptr_final, tokens[++i]);
					}
					break;

				case '-':
					if (ptr_current == ptr_final) {
						zero_add -= tokens[++i];
					}
					else {
						fprintf(outfile, sub_offset, ptr_current - ptr_final, tokens[++i]);
					}
					break;

				case '>':
					ptr_current += tokens[++i];
					break;

				case '<':
					ptr_current -= tokens[++i];
					break;

				case '0':
					if (ptr_current == ptr_final) {
						zero_add = 0;
					}
					fprintf(outfile, zero_offset, ptr_current - ptr_final);
					break;
				}
			}
			if (zero_add == 0) {
				zf_set = 0;
			}
			else {
				fprintf(outfile, add_offset, 0, zero_add);
				zf_set = 1;
			}

			i--;
			break;
		}

		case ',':
			if (prevIO == ',') {
				fprintf(outfile, comma_short);
			}
			else {
				fprintf(outfile, comma);
				prevIO = ',';
			}
			break;

		case '.':
			if (prevIO == '.') {
				fprintf(outfile, dot_short);
			}
			else {
				fprintf(outfile, dot);
				prevIO = '.';
			}
			break;

		case '[':
			if (zf_set) {
				fprintf(outfile, open_short, bracket_count, bracket_count);
			}
			else {
				zf_set = 1;
				fprintf(outfile, open, bracket_count, bracket_count);
			}
			bracket_stack[stack_idx++] = bracket_count++;
			prevIO = '\0';
			break;

		case ']': {
			if (stack_idx == 0) {
				fputs("Error: unbalanced brackets\n", stderr);
				exit(EXIT_FAILURE);
			}
			unsigned x = bracket_stack[--stack_idx];
			if (zf_set) {
				fprintf(outfile, close_short, x, x);
			}
			else {
				fprintf(outfile, close, x, x);
				zf_set = 1;
			}
			prevIO = '\0';
			break;
		}
		}
	}

	if (is_function) {
		fprintf(outfile, footer_f, mem_size);	
	}
	else {
		fprintf(outfile, footer_nf, mem_size);	
	}
	fclose(outfile);
	free(tokens);
	return;
}

static inline void assemble() {
	char command[256];
	sprintf(command, "nasm -felf64 -o %s %s", outfile_name, infile_name);
	puts(command);
	system(command);
	
	sprintf(command, "rm %s", infile_name);
	puts(command);
	system(command);
}

static inline void link() {
	char command[256];
	sprintf(command, "ld -o %s %s", outfile_name, infile_name);
	puts(command);
	system(command);

	sprintf(command, "rm %s", infile_name);
	puts(command);
	system(command);
}

void compile(const char *code, size_t len, Options *opt) {
/*
COMPILE STEP
*/
	//Figure out assembly outfile name
	outfile_name = outbuf;
	if (opt->outfile != NULL && is_enabled(opt, COMPILE_ONLY)) {
		outfile_name = opt->outfile;
	}
	else {
		//Begin with a dot
		outfile_name[0] = '.';
		
		//Copy over everything after the final slash
		char *slash = strrchr(opt->infile, '/');
		slash = slash ? slash + 1 : opt->infile;
		strncpy(outfile_name + 1, slash, 255);
		
		//Change the file extension to .s
		char *dot = strrchr(outfile_name, '.');
		if (dot != outfile_name) {
			*dot = '\0';
		}
		strcat(outfile_name, ".s");
		
		//Get rid of the beginning dot if compile only
		if (is_enabled(opt, COMPILE_ONLY)) {
			outfile_name++;
		}
	}

	printf("compiling %s\n", outfile_name);

	if (is_enabled(opt, OPTIMIZE)) {
		compile_optimized(code, len, opt);
	}
	else {
		compile_unoptimized(code, len, opt);
	}

	if (is_enabled(opt, COMPILE_ONLY)) {
		return;
	}

/*
ASSEMBLE STEP (nasm)
*/
	//assemble using nasm
	infile_name = inbuf;
	strncpy(inbuf, outfile_name, 256);

	//change outfile_name to be .o
	if (opt->outfile != NULL && is_enabled(opt, ASSEMBLE_ONLY)) {
		outfile_name = opt->outfile;
	}
	else {
		char *dot = strrchr(outfile_name, '.');
		*dot = '\0';
		strcat(outfile_name, ".o");
		if (is_enabled(opt, ASSEMBLE_ONLY)) {
			outfile_name++;
		}
	}

	assemble();

	if (is_enabled(opt, ASSEMBLE_ONLY)) {
		return;
	}

/*
LINK STEP (ld)
*/
	//link using ld
	infile_name = inbuf;
	strncpy(inbuf, outfile_name, 256);

	if (opt->outfile == NULL) {
		outfile_name = "a.out";
	}
	else {
		outfile_name = opt->outfile;
	}

	link();
	
	return;
}
