# paths
PREFIX	?= /usr/local/
BINDIR	= ${DESTDIR}${PREFIX}/bin/
MANDIR	= ${DESTDIR}${PREFIX}/man/
MAN1DIR	= ${MANDIR}/man1/

VERSION = 1.1

# compiler
CC	    = cc
CFLAGS	    = -std=c99 -pedantic -Wall -Wextra -g
