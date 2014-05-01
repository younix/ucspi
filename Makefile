CC ?= cc
CFLAGS=-std=c99 -pedantic -Wall -Wextra -g ${GNU} ${BSD}

all: socks tcpclient

socks: socks.o
	${CC} -o $@ socks.o

tcpclient: tcpclient.o
	${CC} -o $@ tcpclient.o

clean:
	rm -f *.core *.o
	rm -f obj/*

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $<

socks.o: socks.c
