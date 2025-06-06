# About
This was my capstone project. It's a Linux CLI application, written in C, for translating brainfuck code into x86-64 assembly for the AMD64 Linux platform.
In addition to compiling standalone programs, it also supports compiling functions which can be linked into larger programs. The programs comes with a help menu, which looks like this:
```
Usage: bf [OPTIONS] FILE

Options:
  -d            Debug the input file
  -i            Interpret the input file
  -S            Compile only (do not assemble or link)
  -c            Assemble only (do not link)
  -O            Compile with optimizations
  -o FILE       Place the output into FILE
  -m SIZE       Set memory size (default: 30000)
  --help        Display this message and exit

Operands:
  FILE          Input file to process

Notes:
  - Exactly one input file must be specified
  - When compiling, memory size is rounded up to the nearest 4K
  - Assembly requires nasm (The Netwide Assembler)
```
# Usage
```
hello.bf:
++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.
```
```
$ bf -i hello.bf
Hello World!
```
```
$ bf hello.bf
$ ./a.out
Hello World!
```
## Functions
```
add8.bf:
%function add8 2
>[<+>-]
```
```
main.c:
#include <stdio.h>

extern char add8(char a, char b);

int main() {
	char a = 5;
	char b = 16;
	char ans = add8(a, b);
	printf("%u + %u = %u\n", a, b, ans);
	return 0;
}
```
```
$ bf -c add8.bf
$ clang -c main.c
$ clang main.o add8.o
$ ./a.out
5 + 16 = 21
```
