/*
 * Copyright (c) 2014-2015 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <tls.h>

#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

/* enviroment */
char **environ;

static void
usage(void)
{
	fprintf(stderr,
	    "tlsc [-hCH] [-c cert_file] [-f ca_file] [-p ca_path] "
	    "program [args...]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[], char *envp[])
{
	int e;
	char buf[BUFSIZ];
	struct tls *tls = NULL;
	int ch;
	environ = envp;

	/* pipes to communicate with the front end */
	int in = STDIN_FILENO;
	int out = STDOUT_FILENO;

	/* pipes to communicate with the back end */
	int sout = 7;
	int sin = 6;

	char *ca_file = NULL;
	char *ca_path = NULL;
	char *cert_file = NULL;
	bool no_verification = false;
	bool no_host_verification = false;
	bool no_cert_verification = false;
	char *host = getenv("TCPREMOTEHOST");
	struct tls_config *tls_config;

	while ((ch = getopt(argc, argv, "c:f:p:n:HCh")) != -1) {
		switch (ch) {
		case 'c':
			if ((cert_file = strdup(optarg)) == NULL) goto err;
			break;
		case 'f':
			if ((ca_file = strdup(optarg)) == NULL) goto err;
			break;
		case 'p':
			if ((ca_path = strdup(optarg)) == NULL) goto err;
			break;
		case 'n':
			if ((host = strdup(optarg)) == NULL) goto err;
			break;
		case 'H':
			no_host_verification = true;
			break;
		case 'C':
			no_cert_verification = true;
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

		if (dup2(pi[PIPE_WRITE], 7) < 0) goto err;
		if (dup2(po_read, 6) < 0) goto err;

		if (close(pi[PIPE_WRITE]) < 0) goto err;
		if (close(po_read) < 0) goto err;
		execve(prog, argv, environ);
	case -1:
		err(EXIT_FAILURE, "tlsc: fork()");
		fprintf(stderr, "tlsc: fork\n");
		goto err;
	}

	in = pi[PIPE_READ];
	out = po[PIPE_WRITE];

	if ((tls_config = tls_config_new()) == NULL)
		err(EXIT_FAILURE, "tls_config_new");

	/* verification settings */
	if (ca_file != NULL)
		tls_config_set_ca_file(tls_config, ca_file);

	if (ca_path != NULL)
		tls_config_set_ca_path(tls_config, ca_path);

	if (cert_file != NULL)
		tls_config_set_cert_file(tls_config, cert_file);

	if (no_cert_verification)
		tls_config_insecure_noverifycert(tls_config);

	if (no_host_verification)
		tls_config_insecure_noverifyhost(tls_config);

	if (no_verification)
		tls_config_insecure_noverifycert(tls_config);

	/* libtls setup */
	if (tls_init() != 0)
		err(EXIT_FAILURE, "tls_init");

	if ((tls = tls_client()) == NULL)
		err(EXIT_FAILURE, "tls_client");

	if (tls_configure(tls, tls_config) != 0)
		err(EXIT_FAILURE, "tls_configure");

	if (tls_connect_fds(tls, sin, sout, host) != 0)
		goto err;

	/* communication loop */
	for (;;) {
		int ret;
		char buf[BUFSIZ];
		size_t n = 0;
		ssize_t sn = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(in, &readfds);
		FD_SET(sin, &readfds);
		int max_fd = MAX(in, sin);

		ret = select(max_fd+1, &readfds, NULL, NULL, NULL);
		if (ret == -1) goto err;

		if (FD_ISSET(sin, &readfds)) {
			do {
 again:
				ret = tls_read(tls, buf, sizeof buf, &n);
				if (ret == TLS_READ_AGAIN)
					goto again;
				if (ret == 0)
					fprintf(stderr, "tls_read(): 0: EOF?\n");
				if (ret == -1)
					goto err;
				if (write(out, buf, n) == -1)
					err(EXIT_FAILURE, "write()");
			} while (n == sizeof buf);
		} else if (FD_ISSET(in, &readfds)) {
			if ((sn = read(in, buf, sizeof buf)) == -1)
				err(EXIT_FAILURE, "read()");
			if (sn == 0)
				err(EXIT_FAILURE, "read(): 0: EOF from inside");
			ret = tls_write(tls, buf, sn, (size_t*)&sn);
		}
	}

	return EXIT_SUCCESS;
 err:
	while ((e = ERR_get_error())) {
		ERR_error_string(e, buf);
		fprintf(stderr, " %s\n", buf);
	}
	err(EXIT_FAILURE, "tls_error: %s", tls_error(tls));
}
