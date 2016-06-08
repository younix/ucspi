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
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
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

void
read_content(size_t content_length, FILE *in, FILE *out)
{
	char buf[BUFSIZ];

	/* handle content */
	for (;content_length > 0;) {
		size_t size = sizeof buf;

		if (content_length < size)
			size = content_length;

		if (fread(buf, size , 1, in) == 0)
			err(EXIT_FAILURE, "fread");

		if (fwrite(buf, size, 1, out) == 0)
			err(EXIT_FAILURE, "fwrite");

		content_length -= size;
	}
}

void
read_content_chunked(FILE *in, FILE *out)
{
	size_t content_length = 0;
	char buf[BUFSIZ];

	do {
		/* read chunk-size [chunk-ext] CRLF */
		if (http_read_line_fh(in, buf, sizeof buf) == -1)
			errx(EXIT_FAILURE, "http_read_line failed");

		int old_errno = errno;
                errno = 0;
		content_length = strtol(buf, NULL, 16);
                if (errno != 0)
                        errx(EXIT_FAILURE, "unreadable chunk size");
                errno = old_errno;

		/* read chunk-data */
		read_content(content_length, in, out);

		/* read CRLF */
		if (http_read_line_fh(in , buf, sizeof buf) == -1)
			errx(EXIT_FAILURE, "http_read_line failed");
	} while (content_length > 0);
}

void
read_header(struct http_response *head, FILE *fh)
{
	char buf[BUFSIZ];

	memset(head, 0, sizeof *head);

	if (http_read_line_fh(fh , buf, sizeof buf) == -1)
		errx(EXIT_FAILURE, "http_read_line failed");

	head->code = http_parse_code(buf, sizeof buf);
	if (head->code != 200)
		errx(EXIT_FAILURE, "%d %s\n", head->code,
		    http_reason_phrase(head->code));

	/* read response header */
	do {
		if (http_read_line_fh(fh, buf, sizeof buf) == -1)
			errx(EXIT_FAILURE, "http_read_line_fh failed");

		if (http_parse_line(head, buf, sizeof buf) == -1)
			errx(EXIT_FAILURE, "http_parse_line failed");
	} while (strcmp(buf, "\r\n") != 0);
}

int
main(int argc, char *argv[])
{
	int ch;
	int verbosity = 0;
	char *host = getenv("TCPREMOTEHOST");
	char *file = NULL;
	char *uri = "/";
	struct http_response head;
	FILE *fh = NULL;
	FILE *out = stdout;

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

	if ((fh = fdopen(READ_FD, "r")) == NULL)
		err(EXIT_FAILURE, "fdopen");

	if (file == NULL)
		file = basename(uri);

	read_header(&head, fh);

	if (head.content_encoding == HTTP_CONT_ENC_GZIP) {
		if ((out = popen("gunzip", "w")) == NULL)
			err(EXIT_FAILURE, "popen");
	}

	if (head.content_encoding == HTTP_CONT_ENC_GZIP)
		read_content_chunked(fh, out);
	else
		read_content(head.content_length, fh, out);

	return EXIT_SUCCESS;
 err:
	perror("httpc:");
	return EXIT_FAILURE;
}
