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

#define PIPE_READ 6
#define PIPE_WRITE 7

enum method {GET, HEAD};
enum encoding {CHUNKED, GZIP};

struct header {
	size_t content_length;
};

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

static char *
read_response(struct header *header)
{
	char buf[BUFSIZ];
	enum state {HEADER_STATE, BODY_STATE};
	enum state state = HEADER_STATE;
	size_t off = 0;
	ssize_t size = 0;

	while ((size = read(PIPE_READ, buf + off, sizeof(buf) - off)) > 0) {
		fprintf(stderr, "head: %s", buf);
		char *line, *next = buf;

		switch (state) {
		case HEADER_STATE:
			line = strsep(&next, "\n");

			if (next == NULL) { /* reached EOL */
				strlcpy(buf, line, sizeof buf);
				off = strlen(line);
				continue;
			}

			parse_header_line(line, header);

			if (strncmp(next, "\r\n", 2) == 0) {
				state = BODY_STATE;
				next+=2;
			} else if(strncmp(next, "\n", 1) == 0) {
				state = BODY_STATE;
				next++;
			} else {
				break;
			}
		case BODY_STATE:
			write(STDOUT_FILENO, buf, size);
			break;
		};

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

	char buf[BUFSIZ];
	/* print request */
	//fprintf(write_fh, "GET %s HTTP/1.1\r\n", uri);
	snprintf(buf, sizeof buf, "GET %s HTTP/1.1\r\n", uri);
	write(PIPE_WRITE, buf, strnlen(buf, sizeof buf));

	if (host != NULL) {
		//fprintf(write_fh, "Host: %s\r\n", host);
		snprintf(buf, sizeof buf, "Host: %s\r\n", host);
		write(PIPE_WRITE, buf, strnlen(buf, sizeof buf));
	}

	//fprintf(write_fh, "\r\n");
	write(PIPE_WRITE, "\r\n", 2);

	/* get response */
	struct header header;
	read_response(&header);

	if (file == NULL)
		file = basename(uri);
//	if (output(file, read_fh) == false) goto err;

	return EXIT_SUCCESS;
 err:
	perror("httpc:");
	return EXIT_FAILURE;
}
