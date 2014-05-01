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
	fprintf(stderr, "tcpclient [-46] HOST PORT PROG\n");
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
	int af = AF_INET6;
	const char *cause = NULL;
	environ = envp;

	/* set some default values */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* parsing command line arguments */
	while ((ch = getopt(argc, argv, "bf:")) != -1) {
		switch (ch) {
		case '4':
			af = AF_INET;
			break;
		case '6':
			af = AF_INET6;
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

	int pi[2];
	int po[2];
	if (pipe(pi) < 0) goto err;
	if (pipe(po) < 0) goto err;
	int rfd = pi[0];
	int wfd = po[1];
	int prog_pid = fork();
	switch (prog_pid) {
	case 0:
		/* start client program */
		if (dup2(pi[1], 6) < 0) goto err;
		if (dup2(po[0], 7) < 0) goto err;
		if (close(rfd) < 0) goto err;
		if (close(wfd) < 0) goto err;
		execve(prog, argv, environ);
	case -1:
		goto err;
	}

	if (close(pi[1]) < 0) goto err;
	if (close(po[0]) < 0) goto err;

	char buf[BUFSIZ];
	ssize_t len = 0;
	fd_set readfds;
	int max = 0;

	fprintf(stderr, "start select\n");
	for (;;) {
		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		if (max < s) max = s;
		FD_SET(rfd, &readfds);
		if (max < rfd) max = rfd;

		if (select(max+1, &readfds, NULL, NULL, NULL) < 0) goto err;
		fprintf(stderr, "selected\n");

		if (FD_ISSET(s, &readfds)) {
			fprintf(stderr, "read from socket\n");
			if ((len = read(s, buf, sizeof buf)) < 0) goto err;
			if (len == 0) { /* connection was closed */
				kill(prog_pid, SIGHUP);
				return EXIT_SUCCESS;
			}
			if ((write(wfd, buf, len)) < len) goto err;
		}
		if (FD_ISSET(rfd, &readfds)) {
			fprintf(stderr, "read from program\n");
			if ((len = read(rfd, buf, sizeof buf)) < 0) goto err;
			if (write(s, buf, len) < len) goto err;
		}
	}

 err:
	perror("tcpclient");
	return EXIT_FAILURE;
}
