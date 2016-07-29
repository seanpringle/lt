CFLAGS=-std=c99
LDFLAGS=-lpcre

all:
	gcc -O2 -c ${CFLAGS} -o arena.o arena.c
	gcc -O2 -c ${CFLAGS} -o op.o op.c
	gcc -O2 -c ${CFLAGS} -o str.o str.c
	gcc -O2 -c ${CFLAGS} -o vec.o vec.c
	gcc -O2 -c ${CFLAGS} -o map.o map.c
	gcc -O2 -c ${CFLAGS} -o parse.o parse.c
	gcc -O2 -c ${CFLAGS} -o lt.o lt.c
	gcc -O2 -flto -o lt arena.o op.o str.o vec.o map.o parse.o lt.o ${LDFLAGS}

dev:
	gcc -Wall -Werror -g -O0 -c ${CFLAGS} -o arena.o arena.c
	gcc -Wall -Werror -g -O0 -c ${CFLAGS} -o op.o op.c
	gcc -Wall -Werror -g -O0 -c ${CFLAGS} -o str.o str.c
	gcc -Wall -Werror -g -O0 -c ${CFLAGS} -o vec.o vec.c
	gcc -Wall -Werror -g -O0 -c ${CFLAGS} -o map.o map.c
	gcc -Wall -Werror -g -O0 -c ${CFLAGS} -o parse.o parse.c
	gcc -Wall -Werror -g -O0 -c ${CFLAGS} -o lt.o lt.c
	gcc -Wall -Werror -g -O0 -flto -o lt arena.o op.o str.o vec.o map.o parse.o lt.o ${LDFLAGS}
