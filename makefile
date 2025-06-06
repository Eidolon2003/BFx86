CC=clang --target=x86_64-pc-linux-gnu -march=native
NAME=bf

# Normal optimized flags
CFLAGS_OPT=-Wall -O3

# Debug/sanitized flags
CFLAGS_SAN=-Wall -O0 -fsanitize=address,leak -g

# Default CFLAGS
CFLAGS=${CFLAGS_OPT}

build: CFLAGS=${CFLAGS_OPT}
build: clean main.o interpret.o option.o compile.o
	${CC} ${CFLAGS} main.o interpret.o option.o compile.o -o ${NAME}

sanitize: CFLAGS=${CFLAGS_SAN}
sanitize: clean main.o interpret.o option.o compile.o
	${CC} ${CFLAGS} main.o interpret.o option.o compile.o -o ${NAME}

install: build
	cp ${NAME} ~/.local/bin

main.o: main.c
	${CC} ${CFLAGS} -c main.c -o main.o

interpret.o: interpret.c interpret.h
	${CC} ${CFLAGS} -c interpret.c -o interpret.o

option.o: option.c option.h
	${CC} ${CFLAGS} -c option.c -o option.o

compile.o: compile.c compile.h
	${CC} ${CFLAGS} -c compile.c -o compile.o

clean:
	rm -f ${NAME} *.o
