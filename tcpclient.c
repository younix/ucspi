/*
 * Copyright (c) 2013-2014 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* enviroment */
char **environ;

void
usage(void)
{
	fprintf(stderr, "tcpclient [-4|6] host port program [args]\n");
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
	char *argv0 = argv[0];
	const char *cause = NULL;
	environ = envp;
	bool h_flag = true;

	/* set some default values */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* parsing command line arguments */
	while ((ch = getopt(argc, argv, "46Hh")) != -1) {
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
		case 'H':
			h_flag = false;
			break;
		case 'h':
			h_flag = true;
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

	/* prepare environment variables */
	char local_ip[NI_MAXHOST] = "";
	char local_host[NI_MAXHOST] = "";
	char local_port[NI_MAXSERV] = "";
	char remote_ip[NI_MAXHOST] = "";
	char remote_host[NI_MAXHOST] = "";
	char remote_port[NI_MAXSERV] = "";

	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof addr;

	/* handle remote address information */
	if (getpeername(s, (struct sockaddr*)&addr, &addrlen) == -1)
		err(EXIT_FAILURE, "getpeername");

	if (h_flag)
		getnameinfo((struct sockaddr *)&addr, addrlen, remote_host,
		    sizeof remote_host, NULL, 0, 0);

	getnameinfo((struct sockaddr *)&addr, addrlen, remote_ip,
	    sizeof remote_ip, remote_port, sizeof remote_port,
	    NI_NUMERICHOST | NI_NUMERICSERV);

	/* handle local address information */
	if (getsockname(s, (struct sockaddr*)&addr, &addrlen) == -1)
		err(EXIT_FAILURE, "getsockname");

	if (h_flag)
		getnameinfo((struct sockaddr *)&addr, addrlen, local_host,
		    sizeof local_host, NULL, 0, 0);

	getnameinfo((struct sockaddr *)&addr, addrlen, local_ip,
	    sizeof local_ip, local_port, sizeof local_port,
	    NI_NUMERICHOST | NI_NUMERICSERV);

	if (strcmp(remote_ip  , "") != 0)setenv("TCPREMOTEIP"  , remote_ip  ,1);
	if (strcmp(remote_port, "") != 0)setenv("TCPREMOTEPORT", remote_port,1);
	if (strcmp(remote_host, "") != 0)setenv("TCPREMOTEHOST", remote_host,1);
	if (strcmp(local_ip   , "") != 0)setenv("TCPLOCALIP"   , local_ip   ,1);
	if (strcmp(local_port , "") != 0)setenv("TCPLOCALPORT" , local_port ,1);
	if (strcmp(local_host , "") != 0)setenv("TCPLOCALHOST" , local_host ,1);

	/* prepare file descriptors */
	if (dup2(s, 6) == -1) goto err;
	if (dup2(s, 7) == -1) goto err;
	if (close(s) == -1) goto err;

	execve(prog, argv, environ);
 err:
	perror(argv0);
	return EXIT_FAILURE;
}
