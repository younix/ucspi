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

#define READ_FD 6
#define WRITE_FD 7

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
	struct tls *tls = NULL;
	int ch;
	environ = envp;

	/* pipes to communicate with the front end */
	int in = -1;
	int out = -1;

	bool no_name_verification = false;
	bool no_cert_verification = false;
	bool no_time_verification = false;
	char *host = getenv("TCPREMOTEHOST");
	struct tls_config *tls_config;

	if (getenv("TLSC_NO_VERIFICATION") != NULL) {
		no_name_verification = true;
		no_cert_verification = true;
		no_time_verification = true;
	}

	if (getenv("TLSC_NO_HOST_VERIFICATION") != NULL)
		no_name_verification = true;

	if (getenv("TLSC_NO_CERT_VERIFICATION") != NULL)
		no_cert_verification = true;

	if (getenv("TLSC_NO_TIME_VERIFICATION") != NULL)
		no_time_verification = true;

	if ((tls_config = tls_config_new()) == NULL)
		err(EXIT_FAILURE, "tls_config_new");

	char *str = NULL;
	if ((str = getenv("TLSC_CERT_FILE")) != NULL)
		if (tls_config_set_cert_file(tls_config, str) == -1)
			err(EXIT_FAILURE, "tls_config_set_cert_file");

	if ((str = getenv("TLSC_KEY_FILE")) != NULL)
		if (tls_config_set_key_file(tls_config, str) == -1)
			err(EXIT_FAILURE, "tls_config_set_key_file");

	if ((str = getenv("TLSC_CA_FILE")) != NULL)
		if (tls_config_set_ca_file(tls_config, str) == -1)
			err(EXIT_FAILURE, "tls_config_set_ca_file");

	if ((str = getenv("TLSC_CA_PATH")) != NULL)
		if (tls_config_set_ca_path(tls_config, str) == -1)
			err(EXIT_FAILURE, "tls_config_set_ca_path");

	while ((ch = getopt(argc, argv, "c:k:f:p:n:HCTVh")) != -1) {
		switch (ch) {
		case 'c':
			if (tls_config_set_cert_file(tls_config, optarg) == -1)
				err(EXIT_FAILURE, "tls_config_set_cert_file");
			break;
		case 'k':
			if (tls_config_set_key_file(tls_config, optarg) == -1)
				err(EXIT_FAILURE, "tls_config_set_key_file");
			break;
		case 'f':
			if (tls_config_set_ca_file(tls_config, optarg) == -1)
				err(EXIT_FAILURE, "tls_config_set_ca_file");
			break;
		case 'p':
			if (tls_config_set_ca_path(tls_config, optarg) == -1)
				err(EXIT_FAILURE, "tls_config_set_ca_path");
			break;
		case 'n':
			if ((host = strdup(optarg)) == NULL) goto err;
			break;
		case 'H':
			no_name_verification = true;
			break;
		case 'C':
			no_cert_verification = true;
			break;
		case 'T':
			no_time_verification = true;
			break;
		case 'V':
			no_name_verification = true;
			no_cert_verification = true;
			no_time_verification = true;
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

	/* verification settings */
	if (no_cert_verification)
		tls_config_insecure_noverifycert(tls_config);

	if (no_name_verification)
		tls_config_insecure_noverifyname(tls_config);

	if (no_time_verification)
		tls_config_insecure_noverifytime(tls_config);

	/* libtls setup */
	if (tls_init() != 0)
		err(EXIT_FAILURE, "tls_init");

	if ((tls = tls_client()) == NULL)
		err(EXIT_FAILURE, "tls_client");

	if (tls_configure(tls, tls_config) != 0)
		err(EXIT_FAILURE, "tls_configure");

	if (tls_connect_fds(tls, READ_FD, WRITE_FD, host) == -1)
		goto err;

	if (tls_handshake(tls) == -1)
		goto err;

	/* overide PROTO to signal the application layer that the communication
	 * channel is save. */
	if (setenv("PROTO", "SSL", 1) == -1)
		err(EXIT_FAILURE, "setenv");

	/* fork front end program */
	char *prog = argv[0];
#	define PIPE_READ 0
#	define PIPE_WRITE 1
	int pi[2];	/* input pipe */
	int po[2];	/* output pipe */
	if (pipe(pi) == -1) err(EXIT_FAILURE, "pipe");
	if (pipe(po) == -1) err(EXIT_FAILURE, "pipe");

	switch (fork()) {
	case -1:
		err(EXIT_FAILURE, "fork");
	case 0: /* client program */

		/* close non-using ends of pipes */
		if (close(pi[PIPE_READ]) == -1) err(EXIT_FAILURE, "close");
		if (close(po[PIPE_WRITE]) == -1) err(EXIT_FAILURE, "close");

		/*
		 * We have to move one descriptor cause po[] may
		 * overlaps with descriptor 6 and 7.
		 */
		int po_read = 0;
		if ((po_read = dup(po[PIPE_READ])) == -1)
			err(EXIT_FAILURE, "dup");
		if (close(po[PIPE_READ]) < 0) err(EXIT_FAILURE, "close");

		if (dup2(pi[PIPE_WRITE], WRITE_FD) < 0)
			err(EXIT_FAILURE, "dup2");
		if (dup2(po_read, READ_FD) < 0) err(EXIT_FAILURE, "dup2");

		if (close(pi[PIPE_WRITE]) < 0) err(EXIT_FAILURE, "close");
		if (close(po_read) < 0) err(EXIT_FAILURE, "close");
		execvpe(prog, argv, environ);
		err(EXIT_FAILURE, "execvpe");
	default: break;	/* parent */
	}

	/* close non-using ends of pipes */
	if (close(pi[PIPE_WRITE]) == -1) err(EXIT_FAILURE, "close");
	if (close(po[PIPE_READ]) == -1) err(EXIT_FAILURE, "close");

	in = pi[PIPE_READ];
	out = po[PIPE_WRITE];

	/* communication loop */
	for (;;) {
		int ret;
		char buf[BUFSIZ];
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
				sn = tls_read(tls, buf, sizeof buf);
				if (sn == TLS_WANT_POLLIN ||
				    sn == TLS_WANT_POLLOUT)
					goto again;
				if (sn == -1)
					goto err;
				if (sn == 0)
					return EXIT_SUCCESS;

				if (write(out, buf, sn) == -1)
					err(EXIT_FAILURE, "write()");
			} while (sn == sizeof buf);
		} else if (FD_ISSET(in, &readfds)) {
			if ((sn = read(in, buf, sizeof buf)) == -1)
				err(EXIT_FAILURE, "read()");
			if (sn == 0)
				goto out;
			if ((sn = tls_write(tls, buf, sn)) == -1)
				goto out;
		}
	}
 out:
	tls_close(tls);
	return EXIT_SUCCESS;
 err:
	errx(EXIT_FAILURE, "tls_error: %s", tls_error(tls));
}
