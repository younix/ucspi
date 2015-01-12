include config.mk

DEFINES += -D_XOPEN_SOURCE=700
DEFINES += -D_BSD_SOURCE
CFLAGS_SSL=`pkg-config --cflags libssl`
LIBS_TLS ?= -ltls `pkg-config --libs libssl`
LIBS_SSL = `pkg-config --libs libssl openssl`

.PHONY: all clean install dist
.SUFFIXES: .c .o

all: socks sslc

socks: socks.o
	$(CC) -o $@ socks.o $(LIBS_BSD)

ucspi-tee: ucspi-tee.o
	$(CC) -static -o $@ ucspi-tee.o

# Just for some tests.  Don't use this.
tcpclient: tcpclient.o
	$(CC) -static -o $@ tcpclient.o

httpc: httpc.o
	$(CC) -static -o $@ httpc.o

sslc: sslc.o
	$(CC) -o sslc sslc.o $(LIBS_SSL) $(LIBS_BSD)

tlsc: tlsc.o
	$(CC) -o tlsc tlsc.o -lssl $(LIBS_TLS) $(LIBS_BSD)

tlss: tlss.o
	$(CC) -o tlss tlss.o $(LIBS_TLS) $(LIBS_BSD)

sslc.o: sslc.c
	$(CC) $(CFLAGS) $(DEFINES) `pkg-config --cflags libssl` -o $@ -c sslc.c

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) -c $<

clean:
	rm -rf *.core *.o obj/* socks tcpclient tlsc sslc httpc ucspi-tools-* ucspi-tee

install: all
	mkdir -p ${DESTDIR}${BINDIR}
	mkdir -p ${DESTDIR}${MAN1DIR}
	install -m 775 socks ${DESTDIR}${BINDIR}
	install -m 775 tlsc ${DESTDIR}${BINDIR}
	install -m 775 ucspi-tee ${DESTDIR}${BINDIR}
	install -m 444 socks.1 ${DESTDIR}${MAN1DIR}
	install -m 444 tlsc.1 ${DESTDIR}${MAN1DIR}

dist: clean
	mkdir -p ucspi-tools-${VERSION}
	cp socks.c socks.1 tlsc.c tlsc.1 README.md config.mk Makefile \
	    ucspi-tools-${VERSION}
	tar czf ucspi-tools-${VERSION}.tar.gz ucspi-tools-${VERSION}
