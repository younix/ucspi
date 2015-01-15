include config.mk

DISTNAME := ucspi-tools-${VERSION}
TARBALL := ${DISTNAME}.tar.gz

DEFINES += -D_XOPEN_SOURCE=700
DEFINES += -D_BSD_SOURCE
CFLAGS_SSL=`pkg-config --cflags libssl`
LIBS_TLS ?= -ltls `pkg-config --libs libssl`
LIBS_SSL = `pkg-config --libs libssl openssl`
TLS ?= tlsc

.PHONY: all clean install
.SUFFIXES: .c .o

all: socks httpc $(TLS) $(TARBALL)

socks: socks.o
	$(CC) -o $@ socks.o $(LIBS_BSD)

ucspi-tee: ucspi-tee.o
	$(CC) -o $@ ucspi-tee.o

# Just for some tests.  Don't use this.
tcpclient: tcpclient.o
	$(CC) -o $@ tcpclient.o

httpc: httpc.o
	$(CC) -o $@ httpc.o

sslc: sslc.o
	$(CC) -o sslc sslc.o $(LIBS_SSL) $(LIBS_BSD)

tlsc: tlsc.o
	$(CC) -o tlsc tlsc.o $(LIBS_TLS) $(LIBS_BSD)

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
	install -m 775 ${TLS} ${DESTDIR}${BINDIR}
	install -m 444 socks.1 ${DESTDIR}${MAN1DIR}
	install -m 444 tlsc.1 ${DESTDIR}${MAN1DIR}

$(TARBALL):
	@mkdir -p $(DISTNAME)
	@cp socks.c socks.1 tlsc.c tlsc.1 httpc.c ucspi-tee.c README.md \
	    config.mk Makefile $(DISTNAME)
	tar czf $(TARBALL) $(DISTNAME)
	@rm -rf $(DISTNAME)

deb: $(TARBALL)
	#dh_make -y -s -i -f $(TARBALL) -p ucspi_${VERSION}
	#rm -f debian/*.ex debian/*.EX debian/README.*
	mkdir debian
	mv downstream/debian/* debian/
	fakeroot debian/rules clean
	fakeroot debian/rules build
	fakeroot debian/rules binary
	debuild -b -us -uc
