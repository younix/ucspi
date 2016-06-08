/*
 * Copyright (c) 2016 Jan Klemkow <j.klemkow@wemelug.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http_parser.h"

#define READ_FD 6
#define WRITE_FD 7

void
usage(void)
{
	fprintf(stderr, "httppc host port prog\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int ch;
	char *basic = NULL;
	char buf[BUFSIZ];

	while ((ch = getopt(argc, argv, "bh")) != -1) {
		switch (ch) {
		case 'b':
			if ((basic = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			/* TODO: transform username and password into basic */
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 3)
		usage();

	char *host = *argv; argc--; argv++;
	char *port = *argv; argc--; argv++;
	char *prog = *argv;

	/* write HTTP request header */
	dprintf(WRITE_FD, "CONNECT %s:%s HTTP/1.1\r\n", host, port);
	dprintf(WRITE_FD, "Host: %s:%s\r\n", host, port);
	if (basic != NULL)
		dprintf(WRITE_FD, "Proxy-Authorization: basic %s\r\n", basic);
	dprintf(WRITE_FD, "\r\n");

	if (http_read_line_fd(READ_FD, buf, sizeof buf) == -1)
		errx(EXIT_FAILURE, "http_read_line_fd failed");

	int code = http_parse_code(buf, sizeof buf);
	if (code != 200)
		errx(EXIT_FAILURE, "%d %s\n", code, http_reason_phrase(code));

	do {
		if (http_read_line_fd(READ_FD, buf, sizeof buf) == -1)
			errx(EXIT_FAILURE, "http_read_line_fd failed");
	} while (strcmp(buf, "\r\n") != 0);

	/* prepare environment variables */
	/* remove all lost address information */
	if (unsetenv("TCPLOCALIP") == -1)
		err(EXIT_FAILURE, "unsetenv");
	if (unsetenv("TCPLOCALPORT") == -1)
		err(EXIT_FAILURE, "unsetenv");
	if (unsetenv("TCPLOCALHOST") == -1)
		err(EXIT_FAILURE, "unsetenv");
	if (unsetenv("TCPREMOTEIP") == -1)
		err(EXIT_FAILURE, "unsetenv");

	/* set new address information */
	if (setenv("TCPREMOTEHOST", host, 1) == -1)
		err(EXIT_FAILURE, "unsetenv");
	if (setenv("TCPREMOTEPORT", port, 1) == -1)
		err(EXIT_FAILURE, "unsetenv");

	/* start client program */                                              
	execvp(prog, argv);
	err(EXIT_FAILURE, "execvp");

	return EXIT_SUCCESS;
}
