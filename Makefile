include config.mk

DISTNAME := ucspi-tools-${VERSION}
TARBALL := ${DISTNAME}.tar.gz

DEFINES += -D_XOPEN_SOURCE=700
DEFINES += -D_BSD_SOURCE
CFLAGS_SSL=`pkg-config --cflags libssl`
LIBS_TLS ?= -ltls `pkg-config --libs libssl`
LIBS_SSL = `pkg-config --libs libssl openssl`

.PHONY: all test clean install
.SUFFIXES: .c .o

all: sockc tlsc tlss httppc httpc

# SOCKS 5
sockc: sockc.o
	$(CC) -o $@ sockc.o $(LIBS_BSD)

socks: socks.o
	$(CC) -o $@ socks.o $(LIBS_BSD)

# HTTP
httpc.o: http_parser.h
http_parser.o: http_parser.h

httpc: httpc.o http_parser.o
	$(CC) -o $@ httpc.o http_parser.o

httppc: httppc.o http_parser.o
	$(CC) -o $@ httppc.o http_parser.o

# SSL/TLS
tlsc: tlsc.o
	$(CC) -o tlsc tlsc.o $(LIBS_TLS) $(LIBS_BSD)

tlss: tlss.o
	$(CC) -o tlss tlss.o $(LIBS_TLS) $(LIBS_BSD)

sslc: sslc.o
	$(CC) -o sslc sslc.o $(LIBS_SSL) $(LIBS_BSD)

sslc.o: sslc.c
	$(CC) $(CFLAGS) $(DEFINES) `pkg-config --cflags libssl` -o $@ -c sslc.c

# Just for some tests.  Don't use this.
ucspi-tee: ucspi-tee.o
	$(CC) -o $@ ucspi-tee.o

tcpc: tcpc.o
	$(CC) -o $@ tcpc.o

tcps: tcps.o
	$(CC) -o $@ tcps.o

splice: splice.o
	$(CC) $(CFLAGS) -o $@ splice.o

findport: findport.o
	$(CC) -o $@ findport.o
# general infrastructure
.c.o:
	$(CC) $(CFLAGS) $(DEFINES) -c $<

clean:
	rm -rf *.core *.o obj/* socks sockc tcpc tcps tlsc tlss sslc httpc \
	    httppc findport ucspi-tools-* ucspi-tee *.key *.csr *.crt *.trace \
	    *.out

install: all
	mkdir -p ${BINDIR}
	mkdir -p ${MAN1DIR}
	install -m 775 sockc ${BINDIR}
	install -m 775 tlsc ${BINDIR}
	install -m 775 tlss ${BINDIR}
	install -m 444 sockc.1 ${MAN1DIR}
	install -m 444 tlsc.1 ${MAN1DIR}
	install -m 444 tlss.1 ${MAN1DIR}

$(TARBALL):
	@mkdir -p $(DISTNAME)
	@cp sockc.c sockc.1 tlsc.c tlsc.1 httpc.c ucspi-tee.c README.md \
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

include tests.mk
