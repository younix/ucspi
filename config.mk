# paths
PREFIX	= /usr/local
BINDIR	= ${PREFIX}/bin
MANDIR	= ${PREFIX}/man
MAN1DIR	= ${MANDIR}/man1/

VERSION = 0.1

# compiler
CC	    = cc -g
CFLAGS	    = -std=c99 -pedantic -Wall -Wextra -g
LDFLAGS_SSL = `pkg-config --libs libssl`
