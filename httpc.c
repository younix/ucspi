/*
 * Copyright (c) 2014-2015 Jan Klemkow <j.klemkow@wemelug.de>
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

#define PIPE_WRITE 6
#define PIPE_READ 7

enum method {GET, HEAD};
enum encoding {CHUNKED, GZIP};

struct header {
	size_t content_length;
};

static bool
output(char *file, FILE *fh)
{
	int fd = STDOUT_FILENO;
	size_t size;
	char buf[BUFSIZ];

	fprintf(stderr, "output()\n");

	if (strcmp(file, "-") != 0 && strcmp(file, "/"))
		if ((fd = open(file, O_WRONLY)) == -1) return false;

	while ((size = fread(buf, sizeof buf, 1, fh)) > 0) {
		fprintf(stderr, "output(): %zd\n", size);
		if (write(fd, buf, size) == -1) return false;
	}

	if (size == -1)
		return false;

	if (close(fd) == -1) return false;
	return true;
}

void
parse_header_line(char *line, struct header *header)
{
	char *value = strchr(line, ':');
	*value = '\0';
	value++;

	if (strcmp(line, "Content-Length") == 0) {
		header->content_length = strtol(value, NULL, 0);
	} else {
		fprintf(stderr, "unknown: %s\n", line);
	}
}

static bool
read_response(struct header *header)
{
	char buf[BUFSIZ];

	while (read(PIPE_READ, buf, sizeof buf) > 0) {
		fprintf(stderr, "head: %s", buf);
		char *eol = strstr(buf, "\r\n");
		eol = '\0';
		char *line = buf;
		parse_header_line(buf, header);
		if (strcmp(buf, "\r\n")) return true;
	}

	return false;
}

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
	char *host = getenv("TCPREMOTEHOST");
	char *file = NULL;
	char *uri = "/";
	FILE *read_fh = NULL;
	FILE *write_fh = NULL;

	if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
		err(EXIT_FAILURE, "setvbuf");

	while ((ch = getopt(argc, argv, "H:o:h")) != -1) {
		switch (ch) {
		case 'H':
			if ((host = strdup(optarg)) == NULL) goto err;
			break;
		case 'o':
			if ((file = strdup(optarg)) == NULL) goto err;
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

	if ((read_fh = fdopen(PIPE_READ, "r")) == NULL)
		err(EXIT_FAILURE, "fopen");

	if ((write_fh = fdopen(PIPE_WRITE, "w")) == NULL)
		err(EXIT_FAILURE, "fopen");

	/* print request */
	fprintf(write_fh, "GET %s HTTP/1.1\r\n", uri);

	if (host != NULL)
		fprintf(write_fh, "Host: %s\r\n", host);

	fprintf(write_fh, "\r\n");
	fflush(write_fh);
	/* get response */
	read_response_header(read_fh);

	if (file == NULL)
		file = basename(uri);
	if (output(file, read_fh) == false) goto err;

	return EXIT_SUCCESS;
 err:
	perror("httpc:");
	return EXIT_FAILURE;
}
