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

static void
usage(void)
{
	fprintf(stderr, "sslc [-i IN] [-o OUT]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	SSL *ssl = NULL;
	SSL_CTX *ssl_ctx = NULL;
	int ch, ret = 1;
	int in = STDIN_FILENO;
	int out = STDOUT_FILENO;

	/* similar to UCSPI protocol */
	int sin = 6;
	int sout = 7;

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

	SSL_load_error_strings();
	SSL_library_init();
	if ((ssl_ctx = SSL_CTX_new(SSLv23_client_method())) == NULL) goto err;
	if ((ssl = SSL_new(ssl_ctx)) == NULL) goto err;
	if (SSL_set_rfd(ssl, sin) == 0) goto err;
	if (SSL_set_wfd(ssl, sout) == 0) goto err;
	if ((ret = SSL_connect(ssl)) < 1) goto err;

	for (;;) {
		char buf[BUFSIZ];
		int n = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(in, &readfds);
		FD_SET(sin, &readfds);
		int max_fd = MAX(in, sin);
		select(max_fd+1, &readfds, NULL, NULL, NULL);

		if (FD_ISSET(sin, &readfds)) {
			if ((n = SSL_read(ssl, buf, BUFSIZ)) <= 0) goto err;
				write(out, buf, n);
		} else if (FD_ISSET(in, &readfds)) {
			if ((n = read(in, buf, BUFSIZ)) > 0)
				SSL_write(ssl, buf, n);
		}
	}
 err:
	perror(__func__);
	if (ret != 1)
		SSL_get_error(ssl, ret);
	ERR_print_errors_fp(stderr);
	return EXIT_FAILURE;
}
