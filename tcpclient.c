/*
 * Copyright (c) 2013 Jan Klemkow <j.klemkow@wemelug.de>
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
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

/* enviroment */
char **environ;

void
usage(void)
{
	fprintf(stderr, "tcpclient [-4|6] HOST PORT PROGRAM [ARGS]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char*argv[], char *envp[])
{
	struct addrinfo hints, *res, *res0;
	int error;
	int save_errno;
	int s;
	int ch;
	const char *cause = NULL;
	environ = envp;

	/* set some default values */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* parsing command line arguments */
	while ((ch = getopt(argc, argv, "46")) != -1) {
		switch (ch) {
		case '4':
			if (hints.ai_family == AF_INET6)
				usage();
			hints.ai_family = AF_INET;
			break;
		case '6':
			if (hints.ai_family == AF_INET)
				usage();
			hints.ai_family = AF_INET6;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 3) usage();
	char *host = *argv; argv++; argc--;
	char *port = *argv; argv++; argc--;
	char *prog = *argv;

	error = getaddrinfo(host, port, &hints, &res0);
	if (error)
		errx(1, "%s", gai_strerror(error));
	s = -1;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}
		if (connect(s, res->ai_addr, res->ai_addrlen) == -1) {
			cause = "connect";
			save_errno = errno;
			close(s);
			errno = save_errno;
			s = -1;
			continue;
		}
		break;  /* okay we got one */
	}
	if (s == -1) goto err;
	freeaddrinfo(res0);

//TODO: set ucspi variables
#	define PIPE_READ 0
#	define PIPE_WRITE 1
	int pi[2];
	int po[2];
	if (pipe(pi) < 0) goto err;
	if (pipe(po) < 0) goto err;

	int prog_pid = fork();
	switch (prog_pid) {
	case 0:	/* start client program */

		if (close(pi[PIPE_READ]) < 0) goto err;
		if (close(po[PIPE_WRITE]) < 0) goto err;

		/*
		 * We have to move one descriptor cause po[] may overlaps with
		 * descriptor 6 and 7.
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

	if (close(pi[PIPE_WRITE]) < 0) goto err;
	if (close(po[PIPE_READ]) < 0) goto err;

	int rfd = pi[PIPE_READ];
	int wfd = po[PIPE_WRITE];

	char buf[BUFSIZ];
	ssize_t len = 0;
	fd_set readfds;
	int max = 0;

	for (;;) {
		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		max = s;
		FD_SET(rfd, &readfds);
		if (max < rfd) max = rfd;

		if (select(max+1, &readfds, NULL, NULL, NULL) < 0) goto err;

		if (FD_ISSET(s, &readfds)) {
			if ((len = recv(s, buf, sizeof buf, 0)) < 0) goto err;
			if (len == 0) { /* connection was closed */
				return EXIT_SUCCESS;
			}
			if ((write(wfd, buf, len)) < len) goto err;
		}
		if (FD_ISSET(rfd, &readfds)) {
			if ((len = read(rfd, buf, sizeof buf)) < 0) goto err;
			if (len == 0)
				err(EXIT_FAILURE, "pipe closed\n");
			if (send(s, buf, len, 0) < len) goto err;
		}
	}

 err:
	perror(cause);
	return EXIT_FAILURE;
}
