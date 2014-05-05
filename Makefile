CC ?= cc
CFLAGS:=-std=c99 -pedantic -Wall -Wextra -g ${GNU} ${BSD}
LSSL:=`pkg-config --libs libssl`

.PHONY: all clean install

all: socks tcpclient sslc

socks: socks.o
	${CC} -static -o $@ socks.o

tcpclient: tcpclient.o
	${CC} -static -o $@ tcpclient.o

sslc: sslc.o
	$(CC) -static -o sslc sslc.o $(LSSL)

clean:
	rm -f *.core *.o obj/* socks tcpclient sslc

install: socks tcpclient sslc
	mkdir -p ${HOME}/bin
	cp socks tcpclient sslc ${HOME}/bin

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $<
