# paths
PREFIX	?= /usr/local/
BINDIR	= ${DESTDIR}${PREFIX}/bin/
MANDIR	= ${DESTDIR}${PREFIX}/man/
MAN1DIR	= ${MANDIR}/man1/

VERSION = 1.7

# compiler
CFLAGS += -std=c99 -pedantic -Wall -Wextra
CFLAGS += -D_XOPEN_SOURCE=700
CFLAGS += -D_BSD_SOURCE
