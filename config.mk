# paths
PREFIX	?= /usr
BINDIR	= ${PREFIX}/bin
MANDIR	= ${PREFIX}/share/man
MAN1DIR	= ${MANDIR}/man1/

VERSION = 1.0

# compiler
CC	    = cc
CFLAGS	    = -std=c99 -pedantic -Wall -Wextra -g
