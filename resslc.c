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

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>

#include <openssl/err.h>
#include <ressl.h>

#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

/* enviroment */
char **environ;

static void
usage(void)
{
	fprintf(stderr, "sslc [-hN] [-f CAFILE] [-p CAPATH] PROGRAM [ARGS]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[], char *envp[])
{
	int e;
	struct ressl *ssl = NULL;
	int ch;
	environ = envp;

	/* pipes to communicate with the front end */
	int in = STDIN_FILENO;
	int out = STDOUT_FILENO;

	/* pipes to communicate with the back end */
	int sout = 6;
	int sin = 7;

	char *ca_file = NULL;
	char *ca_path = NULL;
	//int verify_mode = SSL_VERIFY_PEER;
	char *host = getenv("TCPREMOTEHOST");
	struct ressl_config *ssl_config;
	char buf[BUFSIZ];

	while ((ch = getopt(argc, argv, "f:p:Nh")) != -1) {
		switch (ch) {
		case 'f':
			if ((ca_file = strdup(optarg)) == NULL) goto err;
			break;
		case 'p':
			if ((ca_path = strdup(optarg)) == NULL) goto err;
			break;
		case 'N':
			//verify_mode = SSL_VERIFY_NONE;
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
		fprintf(stderr, "sslc: fork\n");
		goto err;
	}

	in = pi[PIPE_READ];
	out = po[PIPE_WRITE];

	if ((ssl_config = ressl_config_new()) == NULL)
		err(EXIT_FAILURE, "ressl_config_new");

	ressl_config_insecure_noverifycert(ssl_config);

	if (ressl_init() != 0)
		err(EXIT_FAILURE, "ressl_init");

	if ((ssl = ressl_client()) == NULL)
		err(EXIT_FAILURE, "ressl_client");

	if (ressl_configure(ssl, ssl_config) != 0)
		err(EXIT_FAILURE, "ressl_configure");

	host = "www.fefe.de";

	if (ressl_set_fds(ssl, sin, sout) != 0)
		goto err;

	if (ressl_connect_socket(ssl, -1, host) != 0)
		goto err;
		//err(EXIT_FAILURE, "ressl_connect_socket2");

	for (;;) {
		int ret;
		char buf[BUFSIZ];
		size_t n = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(in, &readfds);
		FD_SET(sin, &readfds);
		int max_fd = MAX(in, sin);
		
		fprintf(stderr, "sslc: call select\n");
		if (select(max_fd+1, &readfds, NULL, NULL, NULL) == -1) goto err;

		fprintf(stderr, "sslc: select\n");

		if (FD_ISSET(sin, &readfds)) {
			fprintf(stderr, "sslc: sin!!!!!!!!!!!!!!!!!!!!!!!!!\n");
			do {
				fprintf(stderr, "sslc: do read!\n");
				ret = ressl_read(ssl, buf, sizeof buf, &n);
				fprintf(stderr, "sslc: done read!\n");
				if (ret == -1) goto err;
			} while (ret == RESSL_READ_AGAIN);
			fprintf(stderr, "sslc: write\n");
			write(out, buf, n);
			fprintf(stderr, "sslc: sin?????????????????????????\n");
		} else if (FD_ISSET(in, &readfds)) {
			fprintf(stderr, "sslc: in\n");
			if ((n = read(in, buf, sizeof buf)) <= 0) goto err;
			ressl_write(ssl, buf, n, &n);
		}
	}

	return EXIT_SUCCESS;
 err:

	while ((e = ERR_get_error())) {
		ERR_error_string(e, buf);
		fprintf(stderr, " %s\n", buf);
	}
	err(EXIT_FAILURE, "%s", ressl_error(ssl));
}
