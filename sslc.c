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
#include <stdio.h>
#include <unistd.h>

#include <sys/select.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

/* enviroment */
char **environ;

static void
usage(void)
{
	fprintf(stderr, "sslc [-i IN] [-o OUT]\n");
	fprintf(stderr, "sslc PROGRAM [ARGS]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[], char *envp[])
{
	SSL *ssl = NULL;
	SSL_CTX *ssl_ctx = NULL;
	int ch, ret = 1;
	environ = envp;

	/* pipes to communicate with the front end */
	int in = STDIN_FILENO;
	int out = STDOUT_FILENO;

	/* pipes to communicate with the back end */
	int sout = 6;
	int sin = 7;

	while ((ch = getopt(argc, argv, "i:o:")) != -1) {
		switch (ch) {
		case 'i':
			in = strtol(optarg, NULL, 0);
			if (errno != 0) goto err;
			break;
		case 'o':
			out = strtol(optarg, NULL, 0);
			if (errno != 0) goto err;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* fork front end program */
	if (argc > 0) {
		char *prog = argv[0];
#		define PIPE_READ 0
#		define PIPE_WRITE 1
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
	}

	SSL_load_error_strings();
	SSL_library_init();
	if ((ssl_ctx = SSL_CTX_new(SSLv23_client_method())) == NULL) goto err;
	if ((ssl = SSL_new(ssl_ctx)) == NULL) goto err;
	if (SSL_set_rfd(ssl, sin) == 0) goto err;
	if (SSL_set_wfd(ssl, sout) == 0) goto err;
	if ((ret = SSL_connect(ssl)) < 1) goto err;

	for (;;) {
		int e;
		char buf[BUFSIZ];
		ssize_t n = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(in, &readfds);
		FD_SET(sin, &readfds);
		int max_fd = MAX(in, sin);
		int sel = select(max_fd+1, &readfds, NULL, NULL, NULL);

		if (FD_ISSET(sin, &readfds)) {
			do {
				if ((n = SSL_read(ssl, buf, BUFSIZ)) <= 0) goto err;
				e = SSL_get_error(ssl, n);
				write(out, buf, n);
			} while (e == SSL_ERROR_WANT_READ || n == sizeof buf);
		} else if (FD_ISSET(in, &readfds)) {
			if ((n = read(in, buf, BUFSIZ)) <= 0) goto err;
			SSL_write(ssl, buf, n);
		}
	}
 err:
	if (ret != 1) {
		fprintf(stderr, "check ssl error\n");
		int e = SSL_get_error(ssl, ret);
		fprintf(stderr, "ssl error: %d\n", e);
		ERR_print_errors_fp(stderr);
	}
	perror(__func__);
	return EXIT_FAILURE;
}
