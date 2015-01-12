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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/select.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#define SSL_NAME_LEN 256

#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

/* enviroment */
char **environ;
#if 0
static bool
check_hostname(SSL *ssl, const char *hostname)
{
	X509 *cert;
	char buf[SSL_NAME_LEN];
	if ((cert = SSL_get_peer_certificate(ssl)) == NULL) return false;
	
	X509_NAME_get_text_by_NID(
	    X509_get_subject_name(cert), NID_commonName, buf, sizeof buf);

	buf[SSL_NAME_LEN - 1] = '\0';
	fprintf(stderr, "cert hostname: '%s'\n", buf);

	return true;
}
#endif

static void
usage(void)
{
	fprintf(stderr, "sslc [-hN] [-f CAFILE] [-p CAPATH] PROGRAM [ARGS]\n");
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

	char *ca_file = NULL;
	char *ca_path = NULL;
	int verify_mode = SSL_VERIFY_PEER;

	while ((ch = getopt(argc, argv, "f:p:Nh")) != -1) {
		switch (ch) {
		case 'f':
			if ((ca_file = strdup(optarg)) == NULL) goto err;
			break;
		case 'p':
			if ((ca_path = strdup(optarg)) == NULL) goto err;
			break;
		case 'N':
			verify_mode = SSL_VERIFY_NONE;
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
		goto err;
	}

	in = pi[PIPE_READ];
	out = po[PIPE_WRITE];

	SSL_load_error_strings();
	SSL_library_init();
	if ((ssl_ctx = SSL_CTX_new(SSLv23_client_method())) == NULL) goto err;

	/* prepare certificate checking */
	if (ca_file != NULL || ca_path != NULL)
		SSL_CTX_load_verify_locations(ssl_ctx, ca_file, ca_path);
	else
		SSL_CTX_set_default_verify_paths(ssl_ctx);

	SSL_CTX_set_verify(ssl_ctx, verify_mode, NULL);

	if ((ssl = SSL_new(ssl_ctx)) == NULL) goto err;
	if (SSL_set_rfd(ssl, sin) == 0) goto err;
	if (SSL_set_wfd(ssl, sout) == 0) goto err;
	if ((ret = SSL_connect(ssl)) < 1) goto err;

	//check_hostname(ssl, "www.google.de");

	for (;;) {
		int e;
		char buf[BUFSIZ];
		ssize_t n = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(in, &readfds);
		FD_SET(sin, &readfds);
		int max_fd = MAX(in, sin);
		if (select(max_fd+1, &readfds, NULL, NULL, NULL) == -1) goto err;

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

	return EXIT_SUCCESS;
 err:
	if (ret != 1) {
		int e = SSL_get_error(ssl, ret);
		fprintf(stderr, "sslc: ssl error: %d\n", e);
		ERR_print_errors_fp(stderr);
	}
	perror("sslc");
	return EXIT_FAILURE;
}
