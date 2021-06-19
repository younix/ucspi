include config.mk

DISTNAME := ucspi-tools-${VERSION}
TARBALL := ${DISTNAME}.tar.gz

LIBS_TLS ?= -ltls `pkg-config --libs libssl`

.PHONY: all test clean install
.SUFFIXES: .c .o

all: sockc tlsc tlss httppc httpc https ftpc

# SOCKS 5
#sockc: sockc.o
#	$(CC) $(LDFLAGS) -o $@ sockc.o

#socks: socks.o
#	$(CC) $(LDFLAGS) -o $@ socks.o

# HTTP
httpc.o: http_parser.h
http_parser.o: http_parser.h

httpc: httpc.o http_parser.o
	$(CC) $(LDFLAGS) -o $@ httpc.o http_parser.o

httppc: httppc.o http_parser.o
	$(CC) $(LDFLAGS) -o $@ httppc.o http_parser.o

# SSL/TLS
tlsc: tlsc.o
	$(CC) $(LDFLAGS) -o tlsc tlsc.o $(LIBS_TLS)

tlss: tlss.o
	$(CC) $(LDFLAGS) -o tlss tlss.o $(LIBS_TLS)

# Just for lagacy systems.  Don't use this.
#LIBS_SSL = `pkg-config --libs libssl openssl`
#sslc: sslc.o
#	$(CC) $(LDFLAGS) -o sslc sslc.o $(LIBS_SSL)
#
#sslc.o: sslc.c
#	$(CC) $(CFLAGS) `pkg-config --cflags libssl` -o $@ -c sslc.c

clean:
	rm -rf *.core *.o obj/* socks sockc tcpc tcps tlsc tlss sslc httpc \
	    httppc https ftpc findport ucspi-tools-* ucspi-tee *.key *.csr *.crt \
	    *.trace *.out

install: all
	mkdir -p ${BINDIR}
	mkdir -p ${MAN1DIR}
	install -m 775 sockc ${BINDIR}
	install -m 775 tlsc ${BINDIR}
	install -m 775 tlss ${BINDIR}
	install -m 775 httppc ${BINDIR}
	install -m 444 sockc.1 ${MAN1DIR}
	install -m 444 tlsc.1 ${MAN1DIR}
	install -m 444 tlss.1 ${MAN1DIR}
	install -m 444 httppc.1 ${MAN1DIR}

$(TARBALL):
	git archive --format=tar.gz --prefix=$(DISTNAME)/ HEAD -o $@

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
