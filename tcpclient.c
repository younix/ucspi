#include <err.h>
#include <errno.h>
#include <netdb.h>
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

	if (argc < 3)
		usage();

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
	if (s == -1)
		goto err;
	freeaddrinfo(res0);

//TODO: set ucspi variables

	int pi[2];
	int po[2];
	if (pipe(pi) < 0) goto err;
	if (pipe(po) < 0) goto err;
	int wfd = po[1];
	int rfd = pi[0];

	switch (fork()) {
	case 0:
		/* start client program */
		if (dup2(pi[1], 6) < 0) goto err;
		if (dup2(po[0], 7) < 0) goto err;
		execve(prog, argv, environ);
	case -1:
		goto err;
	}

	char buf[BUFSIZ];
	ssize_t len = 0;
	fd_set readfds;
	int max = 0;

	FD_ZERO(&readfds);

	fprintf(stderr, "s: %d\n", s);

	FD_SET(s, &readfds);
	if (max < s) max = s;

	FD_SET(rfd, &readfds);
	if (max < rfd) max = rfd;

	fprintf(stderr, "start select\n");
	while (select(max+1, &readfds, NULL, NULL, NULL) >= 0) {
		if (FD_ISSET(s, &readfds)) {
			fprintf(stderr, "read from socket\n");
			if ((len = read(s, buf, sizeof buf)) < 0)
				goto err;
			if ((write(wfd, buf, len)) < 0)
				goto err;
		}
		if (FD_ISSET(rfd, &readfds)) {
			fprintf(stderr, "read from program\n");
			if ((len = read(rfd, buf, sizeof buf)) < 0)
				goto err;
			fprintf(stderr, "write to socket: %zd\n", len);
			if ((len = write(s, buf, len)) < 0)
				goto err;
			fprintf(stderr, "written: %zd\n", len);
		}
		fprintf(stderr, "end of select\n");
	}

 err:
	perror("tcpclient");
	return EXIT_FAILURE;
}
