CC=gcc
CFLAGS=-std=c99 -pedantic -Wall -Wextra -g ${GNU} ${BSD}

all: socks

socks: socks.o
	${CC} -o $@ socks.o

clean:
	rm -f socks *.core *.o

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $<

socks.o: socks.c
