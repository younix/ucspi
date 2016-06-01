/*
 * Copyright (c) 2014-2016 Jan Klemkow <j.klemkow@wemelug.de>
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
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http_parser.h"

#define READ_FD 6
#define WRITE_FD 7

static void
usage(void)
{
	fprintf(stderr, "httpc [-h] [-H HOST] [-o file] [URI]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int ch;
	int verbosity = 0;
	char *host = getenv("TCPREMOTEHOST");
	char *file = NULL;
	char *uri = "/";
	char buf[BUFSIZ];
	struct http_response head;
	FILE *fh = NULL;

	if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
		err(EXIT_FAILURE, "setvbuf");

	while ((ch = getopt(argc, argv, "H:o:gvh")) != -1) {
		switch (ch) {
		case 'H':
			if ((host = strdup(optarg)) == NULL) goto err;
			break;
		case 'o':
			if ((file = strdup(optarg)) == NULL) goto err;
			break;
		case 'v':
			verbosity++;
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		uri = argv[0];

	/* write HTTP request header */
	dprintf(WRITE_FD, "GET %s HTTP/1.1\r\n", uri);
	if (host != NULL)
		dprintf(WRITE_FD, "Host: %s\r\n", host);
	dprintf(WRITE_FD, "Accept-Encoding: gzip\r\n", host);
	dprintf(WRITE_FD, "\r\n");

	memset(&head, 0, sizeof head);

	/* read response */
	if ((fh = fdopen(READ_FD, "r")) == NULL)
		err(EXIT_FAILURE, "fopen");

	if (http_read_line_fh(fh , buf, sizeof buf) == -1)
		errx(EXIT_FAILURE, "http_read_line failed");

	int code = http_parse_code(buf, sizeof buf);
	if (code != 200)
		errx(EXIT_FAILURE, "%d %s\n", code, http_reason_phrase(code));

	/* read response header */
	do {
		if (verbosity > 0)
			fputs(buf, stderr);

		if (http_read_line_fh(fh, buf, sizeof buf) == -1)
			errx(EXIT_FAILURE, "http_read_line_fh failed");

		if (http_parse_line(&head, buf, sizeof buf) == -1)
			errx(EXIT_FAILURE, "http_parse_line failed");
	} while (strcmp(buf, "\r\n") != 0);

	/* handle content */
	for (;head.content_lenght > 0;) {
		size_t size = sizeof buf;

		if (head.content_lenght < size)
			size = head.content_lenght;

		if (fread(buf, size , 1, fh) == 0)
			err(EXIT_FAILURE, "fread");

		if (fwrite(buf, size, 1, stdout) == 0)
			err(EXIT_FAILURE, "fwrite");

		head.content_lenght -= size;
	}

	return EXIT_SUCCESS;
 err:
	perror("httpc:");
	return EXIT_FAILURE;
}
