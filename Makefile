CFLAGS=-Wall -Werror -c -g -std=c99 -O2
LDFLAGS=-lpcre

all:
	gcc ${CFLAGS} -o arena.o arena.c
	gcc ${CFLAGS} -o op.o op.c
	gcc ${CFLAGS} -o str.o str.c
	gcc ${CFLAGS} -o vec.o vec.c
	gcc ${CFLAGS} -o map.o map.c
	gcc ${CFLAGS} -o parse.o parse.c
	gcc ${CFLAGS} -o lt.o lt.c
	gcc -O2 -flto -o lt arena.o op.o str.o vec.o map.o parse.o lt.o ${LDFLAGS}
