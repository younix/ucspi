#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* negotiation fields */
#define SOCKSv5 0x05
#define RSV 0x00

/* authentication methods */
#define NO_AUTH 0x00
#define GSSAPI 	0x01
#define USRPASS 0x02
#define NOT_ACC 0xFF

/* address types */
#define IPv4 0x01
#define BIND 0x03
#define IPv6 0x04

struct nego {
	uint8_t ver;
	uint8_t nmethods;
	uint8_t* methods;
};

struct nego_ans {
	uint8_t ver;
	uint8_t method;
};

struct reqest {
        uint8_t   ver;
	uint8_t   cmd;
	uint8_t   rsv;
	uint8_t   atyp;
	uint8_t  *dst_addr;
	uint16_t  dst_port;
};

struct user_auth {
	uint8_t  ver;
	uint8_t  ulen;
	char    *uname;
	uint8_t  plen;
	char    *passwd;
};
void
usage(void)
{
	fprintf(stderr, "socks [-u USER] [-p PASS] HOST PORT PROGRAM [PROGRAM ARGS...]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	struct nego nego = {SOCKSv5, 0x01, 0x00};
	struct nego_ans nego_ans;
	struct reqest request = {SOCKSv5, 0x00, RSV, 0,0,0};
	struct user_auth user_auth = {SOCKSv5, 0x00, '\0', 0x00, '\0'};
	int ch;

	while ((ch = getopt(argc, argv, "u:p:")) != -1) {
		switch (ch) {
		case 'u':
			if (strlen(optarg) > 255)
				return EXIT_FAILURE;
			user_auth.ulen = strlen(optarg);
			user_auth.uname = strdup(optarg);
			break;
		case 'p':
			if (strlen(optarg) > 255)
				return EXIT_FAILURE;
			user_auth.plen = strlen(optarg);
			user_auth.passwd = strdup(optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;


	write(STDOUT_FILENO, &nego, sizeof nego);
	read(STDOUT_FILENO, &nego_ans, sizeof nego_ans);

	if (nego_ans.method == NOT_ACC)
		return EXIT_FAILURE;

	write(STDOUT_FILENO, &request, sizeof request);

	return EXIT_SUCCESS;
}
