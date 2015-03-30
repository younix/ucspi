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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tls.h>
#include <openssl/err.h>

#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

/* ucspi */
#define READ_FD STDIN_FILENO
#define WRITE_FD STDOUT_FILENO

void
usage(void)
{
	fprintf(stderr, "tlss [-c cert_file] [-k key_file] prog [args]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct tls *tls = NULL;
	struct tls *cctx = NULL;
	struct tls_config *tls_config = NULL;
	char *cert_file = NULL;
	char *key_file = NULL;
	char buf[BUFSIZ];
	int ch;
	int e;

	while ((ch = getopt(argc, argv, "c:k:")) != -1) {
		switch (ch) {
		case 'c':
			if ((cert_file = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		case 'k':
			if ((key_file = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* prepare libtls */
	if (tls_init() == -1)
		err(EXIT_FAILURE, "tls_init");

	if ((tls = tls_server()) == NULL)
		err(EXIT_FAILURE, "tls_server");

	if ((tls_config = tls_config_new()) == NULL)
		err(EXIT_FAILURE, "tls_config_new");

	if (tls_config_set_key_file(tls_config, key_file) == -1)
		err(EXIT_FAILURE, "tls_config_set_key_file");

	if (tls_config_set_cert_file(tls_config, cert_file) == -1)
		err(EXIT_FAILURE, "tls_config_set_cert_file");

	if (tls_configure(tls, tls_config) == -1)
		goto err;

	if (tls_accept_fds(tls, &cctx, STDIN_FILENO, STDOUT_FILENO) == -1)
		goto err;

	if (setenv("PROTO", "SSL", 1) == -1)
		err(EXIT_FAILURE, "setenv");

	/* fork front end program */
	char *prog = argv[0];
#	define PIPE_READ 0
#	define PIPE_WRITE 1
	int pi[2];      /* input pipe */
	int po[2];      /* output pipe */
	if (pipe(pi) == -1) err(EXIT_FAILURE, "pipe");
	if (pipe(po) == -1) err(EXIT_FAILURE, "pipe");

	switch (fork()) {
	case -1:
		err(EXIT_FAILURE, "fork");
	case 0: /* client program */

		/* close non-using ends of pipes */
		if (close(pi[PIPE_READ]) == -1) err(EXIT_FAILURE, "close");
		if (close(po[PIPE_WRITE]) == -1) err(EXIT_FAILURE, "close");

		/* move pipe end to ucspi defined fd numbers */
		if (dup2(po[PIPE_READ], READ_FD) == -1)
			err(EXIT_FAILURE, "dup2");
		if (dup2(pi[PIPE_WRITE], WRITE_FD) == -1)
			err(EXIT_FAILURE, "dup2");

		if (close(po[PIPE_READ]) == -1) err(EXIT_FAILURE, "close");
		if (close(pi[PIPE_WRITE]) == -1) err(EXIT_FAILURE, "close");

		execv(prog, argv);
		err(EXIT_FAILURE, "execve");
	default: break; /* parent */
	}

	/* close non-using ends of pipes */
	if (close(pi[PIPE_WRITE]) == -1) err(EXIT_FAILURE, "close");
	if (close(po[PIPE_READ]) == -1) err(EXIT_FAILURE, "close");

	int in = pi[PIPE_READ];
	int out = po[PIPE_WRITE];

	/* communication loop */
	for (;;) {
		int ret;
		char buf[BUFSIZ];
		size_t n = 0;
		ssize_t sn = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(in, &readfds);
		FD_SET(READ_FD, &readfds);
		int max_fd = MAX(in, READ_FD);

		ret = select(max_fd+1, &readfds, NULL, NULL, NULL);
		if (ret == -1)
			err(EXIT_FAILURE, "select");

		if (FD_ISSET(READ_FD, &readfds)) {
			do {
 again:
				ret = tls_read(cctx, buf, sizeof buf, &n);
				if (ret == TLS_READ_AGAIN)
					goto again;
				if (ret == -1) /* XXX: unable to detect EOF */
					goto out;
				if (write(out, buf, n) == -1)
					err(EXIT_FAILURE, "write()");
			} while (n == sizeof buf);
		} else if (FD_ISSET(in, &readfds)) {
			if ((sn = read(in, buf, sizeof buf)) == -1)
				err(EXIT_FAILURE, "read()");
			if (sn == 0) /* EOF from inside */
				goto out;
			/* XXX: unable to detect disconnect here */
			if (tls_write(cctx, buf, sn, (size_t *)&sn) == -1)
				goto out;
		}
	}

 out:
	tls_close(cctx);
	return EXIT_SUCCESS;
 err:
	while ((e = ERR_get_error())) {
		ERR_error_string(e, buf);
		fprintf(stderr, " %s\n", buf);
	}
	errx(EXIT_FAILURE, "tls_error: %s", tls_error(tls));
}
