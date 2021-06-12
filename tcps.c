/*
 * Copyright (c) 2015 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXSOCK 10
#define MAX(a, b) ((a) < (b) ? (b) : (a))

/* Set enviroment variable if value is not empty. */
#define set_env(name, value)			\
	if (strcmp((value), "") != 0)		\
		setenv((name), (value), 1)

struct sock {
	int s;
	char ip[NI_MAXHOST];
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];
};

void
start_prog(struct sock *sock, char *prog, char *argv[])
{
	int ecode = 0;
	int s = -1;
	struct sockaddr_storage addr;
	socklen_t len = sizeof addr;
	char ip[NI_MAXHOST] = "";
	char host[NI_MAXHOST] = "";
	char serv[NI_MAXSERV] = "";

	if ((s = accept(sock->s, (struct sockaddr *)&addr, &len)) == -1)
		err(EXIT_FAILURE, "accept");

	switch (fork()) {
	case -1: err(EXIT_FAILURE, "fork()");	/* error */
	case  0: break;				/* child */
	default:				/* parent */
		if (close(s) == -1)
			err(EXIT_FAILURE, "close");
		return;
	}

	/* get remote address information */
	if ((ecode = getnameinfo((struct sockaddr *)&addr, len, ip, sizeof ip,
	    serv, sizeof serv, NI_NUMERICHOST|NI_NUMERICSERV)) != 0)
		errx(EXIT_FAILURE, "getnameinfo: %s", gai_strerror(ecode));

	if ((ecode = getnameinfo((struct sockaddr *)&addr, len, host,
	    sizeof host, NULL, 0, 0)) != 0)
		errx(EXIT_FAILURE, "getnameinfo: %s", gai_strerror(ecode));

	/* prepare enviroment */
	set_env("TCPREMOTEIP"  , ip);
	set_env("TCPREMOTEHOST", host);
	set_env("TCPREMOTEPORT", serv);

	set_env("TCPLOCALIP"  , sock->ip);
	set_env("TCPLOCALHOST", sock->host);
	set_env("TCPLOCALPORT", sock->serv);
	set_env("PROTO", "TCP");

	/* prepare file descriptors */
	if (dup2(s, STDIN_FILENO) == -1) err(EXIT_FAILURE, "dup2");
	if (dup2(s, STDOUT_FILENO) == -1) err(EXIT_FAILURE, "dup2");
	if (close(s) == -1) err(EXIT_FAILURE, "close");

	/* execute program */
	execvp(prog, argv);
	err(EXIT_FAILURE, "execvp()");
}

static void
usage(void)
{
	fprintf(stderr, "tcps [-46h] address port program [args]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int ch;
	bool debug = false;

	struct addrinfo hints, *res, *res0;
	int error;
	int save_errno;
	struct sock sock[MAXSOCK];
	int nsock;
	const char *cause = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	while ((ch = getopt(argc, argv, "46dh")) != -1) {
		switch (ch) {
		case '4':
			hints.ai_family = PF_INET;
			break;
		case '6':
			hints.ai_family = PF_INET6;
			break;
		case 'd':
			debug = true;
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

	char *host = *argv; argv++; argc--;
	char *port = *argv; argv++; argc--;
	char *prog = *argv;

	if ((error = getaddrinfo(host, port, &hints, &res0)) != 0)
		errx(EXIT_FAILURE, "getaddrinfo: %s", gai_strerror(error));

	nsock = 0;
	for (res = res0; res && nsock < MAXSOCK; res = res->ai_next) {
		sock[nsock].s = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (sock[nsock].s == -1) {
			cause = "socket";
			continue;
		}

		if (bind(sock[nsock].s, res->ai_addr, res->ai_addrlen) == -1) {
			cause = "bind";
			save_errno = errno;
			close(sock[nsock].s);
			errno = save_errno;
			continue;
		}

		if (listen(sock[nsock].s, 5) == -1)
			err(EXIT_FAILURE, "listen");

		/* get really used address information */
		if (getsockname(sock[nsock].s, res->ai_addr, &res->ai_addrlen)
		    == -1)
			err(EXIT_FAILURE, "getsockname");

		/* resolve local address information */
		if ((error = getnameinfo(res->ai_addr, res->ai_addrlen,
		    sock[nsock].ip  , sizeof sock[nsock].ip,
		    sock[nsock].serv, sizeof sock[nsock].serv,
		    NI_NUMERICHOST|NI_NUMERICSERV)) != 0)
			errx(EXIT_FAILURE, "getnameinfo: %s",
			    gai_strerror(error));

		if ((error = getnameinfo(res->ai_addr, res->ai_addrlen,
		    sock[nsock].host, sizeof sock[nsock].host, NULL, 0, 0)) !=0)
			errx(EXIT_FAILURE, "getnameinfo: %s",
			    gai_strerror(error));

		if (debug)
			fprintf(stderr, "listen: %s:%s\n", sock[nsock].ip,
			    sock[nsock].serv);

		nsock++;
	}
	if (nsock == 0)
		err(EXIT_FAILURE, "%s", cause);
	freeaddrinfo(res0);

	/* select loop */
	for (;;) {
		int max = 0;
		fd_set fdset;

		FD_ZERO(&fdset);

		for (int i = 0; i < nsock; i++) {
			FD_SET(sock[i].s, &fdset);
			max = MAX(max, sock[i].s);
		}

		if (select(max+1, &fdset, NULL, NULL, 0) == -1)
			err(EXIT_FAILURE, "select");

		for (int i = 0; i < nsock; i++)
			if (FD_ISSET(sock[i].s, &fdset))
				start_prog(&sock[i], prog, argv);
	}

	return EXIT_SUCCESS;
}
