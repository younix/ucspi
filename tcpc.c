/*
 * Copyright (c) 2013-2015 Jan Klemkow <j.klemkow@wemelug.de>
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
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
set_local_addr(int s, int family, char *local_addr_str, char *local_port_str)
{
	struct sockaddr_storage ia;
	struct sockaddr_in *sa4 = (struct sockaddr_in *)&ia;
	struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&ia;
	socklen_t slen = 0;
	int local_port = 0;

	if (local_port_str != NULL) {
		local_port = strtol(local_port_str, NULL, 0);
		if (errno != 0)
			err(EXIT_FAILURE, "strtol");
	}

	memset(&ia, 0, sizeof ia);
	ia.ss_family = family;

	switch (ia.ss_family) {
	case PF_INET:
		slen = sizeof *sa4;
		sa4->sin_port = htons(local_port);
		break;
	case PF_INET6:
		slen = sizeof *sa6;
		sa6->sin6_port = htons(local_port);
		break;
	default:
		errx(EXIT_FAILURE, "unknown protocol family: %d", family);
	}

	if (local_addr_str != NULL) {
		int ret = 0;
		ret = inet_pton(ia.ss_family, local_addr_str,
		    ia.ss_family == PF_INET ?  (void*)&sa4->sin_addr :
			(void*)&sa6->sin6_addr);
		if (ret == -1)
			err(EXIT_FAILURE, "inet_pton");

		if (ret == 0)
			errx(EXIT_FAILURE, "unable to parse local ip address");
	}

	return bind(s, (struct sockaddr *)&ia, slen);
}

void
usage(void)
{
	fprintf(stderr, "tcpclient [-4|6] [-Hh] host port program [args]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char*argv[])
{
	struct addrinfo hints, *res, *res0;
	int error = 0;
	int save_errno;
	int s;
	int ch;
	char *argv0 = argv[0];
	const char *cause = NULL;
	bool h_flag = true;

	/* set some default values */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char *local_addr_str = NULL;
	char *local_port_str = NULL;

	/* parsing command line arguments */
	while ((ch = getopt(argc, argv, "46Hhi:p:")) != -1) {
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
		case 'i':
			if ((local_addr_str = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		case 'p':
			if ((local_port_str = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
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
		errx(EXIT_FAILURE, "%s", gai_strerror(error));
	s = -1;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}

		/* set local address information */
		if (local_addr_str != NULL || local_port_str != NULL)
			if (set_local_addr(s, res->ai_family, local_addr_str,
			   local_port_str) == -1) {
				cause = "bind";
				save_errno = errno;
				close(s);
				errno = save_errno;
				s = -1;
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
		if ((error = getnameinfo((struct sockaddr *)&addr, addrlen,
		    remote_host, sizeof remote_host, NULL, 0, 0)) != 0)
			errx(EXIT_FAILURE, "%s", gai_strerror(error));

	if ((error = getnameinfo((struct sockaddr *)&addr, addrlen, remote_ip,
	    sizeof remote_ip, remote_port, sizeof remote_port,
	    NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
		errx(EXIT_FAILURE, "%s", gai_strerror(error));

	/* handle local address information */
	if (getsockname(s, (struct sockaddr*)&addr, &addrlen) == -1)
		err(EXIT_FAILURE, "getsockname");

	if (h_flag)
		if ((error = getnameinfo((struct sockaddr *)&addr, addrlen,
		    local_host, sizeof local_host, NULL, 0, 0)) != 0)
			errx(EXIT_FAILURE, "%s", gai_strerror(error));

	if ((error = getnameinfo((struct sockaddr *)&addr, addrlen, local_ip,
	    sizeof local_ip, local_port, sizeof local_port,
	    NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
		errx(EXIT_FAILURE, "%s", gai_strerror(error));

	if (strcmp(remote_ip  , "") != 0)setenv("TCPREMOTEIP"  , remote_ip  ,1);
	if (strcmp(remote_port, "") != 0)setenv("TCPREMOTEPORT", remote_port,1);
	if (strcmp(remote_host, "") != 0)setenv("TCPREMOTEHOST", remote_host,1);
	if (strcmp(local_ip   , "") != 0)setenv("TCPLOCALIP"   , local_ip   ,1);
	if (strcmp(local_port , "") != 0)setenv("TCPLOCALPORT" , local_port ,1);
	if (strcmp(local_host , "") != 0)setenv("TCPLOCALHOST" , local_host ,1);
	setenv("PROTO", "TCP", 1);

	/* prepare file descriptors */
	if (dup2(s, 6) == -1) err(EXIT_FAILURE, "dup2");
	if (dup2(s, 7) == -1) err(EXIT_FAILURE, "dup2");
	if (close(s) == -1) err(EXIT_FAILURE, "close");

	execvp(prog, argv);
	err(EXIT_FAILURE, "execve");
 err:
	perror(argv0);
	return EXIT_FAILURE;
}
