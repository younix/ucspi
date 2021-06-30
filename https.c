/*
 * Copyright (c) 2021 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define S_(x)	#x
#define S(x)	S_(x)

#define respond(str) do {				\
	fputs("HTTP/1.1 " str "\r\n\r\n", stdout);	\
	exit(EXIT_SUCCESS);				\
} while (0)

int
main(void)
{
	char buf[BUFSIZ];
	char method[BUFSIZ+1];
	char path[PATH_MAX+1];
	char file[PATH_MAX];
	char htdocs[PATH_MAX] = "/var/www/htdocs";
	char resolved[PATH_MAX];
	char host[HOST_NAME_MAX+1];
	enum connection { KEEP_ALIVE, CLOSE } connection;
	struct stat sb;
	unsigned int major;
	unsigned int minor;
	size_t n;
	FILE *fh;

#ifdef __OpenBSD__
	if (unveil(htdocs, "r") == -1)
		respond("500 Internal Server Error");
	if (pledge("stdio rpath", NULL) == -1)
		respond("500 Internal Server Error");
#endif
 next:
	connection = CLOSE;
	memset(host, 0, sizeof host);
	memset(&sb, 0, sizeof sb);
	memset(path, 0, sizeof path);
	memset(resolved, 0, sizeof resolved);

	/* parse method */
	if (scanf("%" S(BUFSIZ) "s %" S(PATH_MAX) "s HTTP/%u.%u\r\n",
	    method, path, &major, &minor) != 4)
		respond("400 Bad Request");

	/* parse header fields */
	while (fgets(buf, sizeof buf, stdin) != NULL &&
	    strcmp(buf, "\r\n") != 0) {
		if (strcmp(buf, "Connection: keep-alive\r\n") == 0)
			connection = KEEP_ALIVE;
		if (sscanf(buf, "Host: %" S(HOST_NAME_MAX) "s\r\n", host) == 1)
			continue;
	}

	/* check for default file */
	if (strcmp(path, "/") == 0)
		strcpy(path, "index.html");

	snprintf(file, sizeof file, "%s/%s/%s", htdocs, host, path);
	if (realpath(file, path) == NULL) {
		if (errno == ENOENT)
			respond("404 Not Found");
		respond("400 Bad Request");
	}
	/* check that realpath is inside htdocs */
	if (strncmp(htdocs, file, strlen(htdocs)) != 0)
		respond("400 Bad Request");

	if ((fh = fopen(path, "r")) == NULL)
		respond("400 Bad Request");

	if (fstat(fileno(fh), &sb) == -1)
		respond("500 Internal Server Error");

	/* response header */
	fputs("HTTP/1.1 200 OK\r\n", stdout);
	printf("Content-Length: %lld\r\n", sb.st_size);
	fputs("\r\n", stdout);

	/* transfer body */
	while ((n = fread(buf, sizeof *buf, sizeof buf, fh)) > 0)
		if (fwrite(buf, sizeof *buf, n, stdout) == 0)
			return EXIT_FAILURE;

	if (fclose(fh) == EOF)
		return EXIT_FAILURE;

	if (fflush(stdout) == EOF)
		err(EXIT_FAILURE, "fflush");

	if (connection == KEEP_ALIVE)
		goto next;

	return EXIT_SUCCESS;
}
