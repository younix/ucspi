/*
 * Copyright (c) 2014 Jan Klemkow <j.klemkow@wemelug.de>
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

#define _XOPEN_SOURCE 700
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/select.h>

#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

/* enviroment */
char **environ;

static void
usage(void)
{
	fprintf(stderr, "ucspi-tee PROGRAM [ARGS]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[], char *envp[])
{
	int ch;
	environ = envp;

	/* pipes to communicate with the front end */
	int in = STDIN_FILENO;
	int out = STDOUT_FILENO;

	/* pipes to communicate with the back end */
	int sout = 6;
	int sin = 7;

	char *in_file = NULL;
	char *out_file = NULL;

	int in_fd = STDERR_FILENO;
	int out_fd = STDERR_FILENO;

	while ((ch = getopt(argc, argv, "f:p:Nh")) != -1) {
		switch (ch) {
		case 'i':
			if ((in_file = strdup(optarg)) == NULL) goto err;
			break;
		case 'o':
			if ((out_file = strdup(optarg)) == NULL) goto err;
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	/* fork front end program */
	char *prog = argv[0];
#	define PIPE_READ 0
#	define PIPE_WRITE 1
	int pi[2];
	int po[2];
	if (pipe(pi) < 0) goto err;
	if (pipe(po) < 0) goto err;
	switch (fork()) {
	case 0: /* start client program */
		if (close(pi[PIPE_READ]) < 0) goto err;
		if (close(po[PIPE_WRITE]) < 0) goto err;

		/*
		 * We have to move one descriptor cause po[] may
		 * overlaps with descriptor 6 and 7.
		 */
		int po_read = 0;
		if ((po_read = dup(po[PIPE_READ])) < 0) goto err;
		if (close(po[PIPE_READ]) < 0) goto err;

		if (dup2(pi[PIPE_WRITE], 6) < 0) goto err;
		if (dup2(po_read, 7) < 0) goto err;

		if (close(pi[PIPE_WRITE]) < 0) goto err;
		if (close(po_read) < 0) goto err;
		execve(prog, argv, environ);
	case -1:
		goto err;
	}

	in = pi[PIPE_READ];
	out = po[PIPE_WRITE];

	for (;;) {
		char buf[BUFSIZ];
		ssize_t n = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(in, &readfds);
		FD_SET(sin, &readfds);
		int max_fd = MAX(in, sin);
		if (select(max_fd+1, &readfds, NULL, NULL, NULL) == -1) goto err;

		if (FD_ISSET(sin, &readfds)) {
			if ((n = read(sin, buf, BUFSIZ)) <= 0) goto err;
			if (write(out, buf, n) < n) goto err; 
			write(in_fd, "server:\n", 8);
			if (write(in_fd, buf, n) < n) goto err; 
		} else if (FD_ISSET(in, &readfds)) {
			if ((n = read(in, buf, BUFSIZ)) <= 0) goto err;
			if (write(sout, buf, n) < n) goto err; 
			write(in_fd, "client:\n", 8);
			if (write(out_fd, buf, n) < n) goto err; 
		}
	}

	return EXIT_SUCCESS;
 err:
	perror("ucspi-tee");
	return EXIT_FAILURE;
}
