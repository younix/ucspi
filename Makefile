include config.mk

DEFINES += -D_XOPEN_SOURCE=700
DEFINES += -D_BSD_SOURCE
CFLAGS_SSL=`pkg-config --cflags libssl`
LIBS_SSL= -lressl `pkg-config --libs libssl`

.PHONY: all clean install dist
.SUFFIXES: .c .o

all: socks sslc ucspi-tee resslc httpc

socks: socks.o
	$(CC) -static -o $@ socks.o $(LIBS_BSD)

ucspi-tee: ucspi-tee.o
	$(CC) -static -o $@ ucspi-tee.o

# Just for some tests.  Don't use this.
tcpclient: tcpclient.o
	$(CC) -static -o $@ tcpclient.o

httpc: httpc.o
	$(CC) -static -o $@ httpc.o

sslc: sslc.o
	$(CC) -o sslc sslc.o $(LIBS_SSL) $(LIBS_BSD)

resslc: resslc.o
	$(CC) -o resslc resslc.o $(LIBS_SSL) $(LIBS_BSD)

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) -c $<

clean:
	rm -rf *.core *.o obj/* socks tcpclient resslc sslc httpc ucspi-tools-*

install: all
	mkdir -p ${DESTDIR}${BINDIR}
	mkdir -p ${DESTDIR}${MAN1DIR}
	install -m 775 socks ${DESTDIR}${BINDIR}
	install -m 775 sslc ${DESTDIR}${BINDIR}
	install -m 444 socks.1 ${DESTDIR}${MAN1DIR}
	install -m 444 sslc.1 ${DESTDIR}${MAN1DIR}

dist: clean
	mkdir -p ucspi-tools-${VERSION}
	cp socks.c socks.1 sslc.c sslc.1 README.md config.mk Makefile \
	    ucspi-tools-${VERSION}
	tar czf ucspi-tools-${VERSION}.tar.gz ucspi-tools-${VERSION}
