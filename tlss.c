#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tls.h>

void
usage(void)
{
	fprintf(stderr, "tlss [-c cert_file] prog [args]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct tls *tls = NULL;
	struct tls_config *tls_config = NULL;
	char *password = NULL;
	uint8_t *cert = NULL;
	size_t cert_len = 0;
	char *cert_file = NULL;
	int ch;

	while ((ch = getopt(argc, argv, "c:")) != -1) {
		switch (ch) {
		case 'c':
			if ((cert_file = strdup(optarg)) == NULL)
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

	if ((tls_config = tls_config_new()) == NULL)
		err(EXIT_FAILURE, "tls_config_new");

	if ((tls = tls_server()) == NULL)
		err(EXIT_FAILURE, "tls_server");

	if ((cert = tls_load_file(cert_file, &cert_len, password)) == NULL)
		err(EXIT_FAILURE, "tls_load_file");

	tls_accept_fds(tls, NULL, STDIN_FILENO, STDOUT_FILENO);	

	return EXIT_SUCCESS;
}
