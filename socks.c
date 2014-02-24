#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* uscpi */
#define WRITE_FD 6
#define READ_FD 7

/* negotiation fields */
#define SOCKSv5 0x05
#define RSV 0x00

/* authentication methods */
#define NO_AUTH 0x00
#define GSSAPI 	0x01	/* not supported */
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

struct request {
        uint8_t  ver;
	uint8_t  cmd;
	uint8_t  rsv;
	uint8_t  atyp;
	char    *addr;
	uint16_t port;
};

struct reply {
	uint8_t  ver;
	uint8_t  rep;
	uint8_t  rsv;
	uint8_t  atyp;
	char    *addr;
	uint16_t port;
};

struct user_auth {
	uint8_t  ver;
	uint8_t  ulen;
	char    *uname;
	uint8_t  plen;
	char    *passwd;
};

char *
rep_mesg(uint8_t rep)
{
	switch (rep) {
	case 0x00: return "succeeded";
	case 0x01: return "general SOCKS server failure";
	case 0x02: return "connection not allowed by ruleset";
	case 0x03: return "Network unreachable";
	case 0x04: return "Host unreachable";
	case 0x05: return "Connection refused";
	case 0x06: return "TTL expired";
	case 0x07: return "Command not supported";
	case 0x08: return "Address type not supported";
	case 0x09:
	default: break;
	}

	return "unassigned";
}

void
usage(void)
{
	fprintf(stderr, "socks [-u USER] [-p PASS] "
			"HOST PORT PROGRAM [PROGRAM ARGS...]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	struct nego nego = {SOCKSv5, 0x01, 0};
	struct nego_ans nego_ans;
	struct request request = {SOCKSv5, 0, RSV, 0,0,0};
	struct reply reply = {SOCKSv5, 0, RSV, 0, 0, 0};
	struct user_auth user_auth = {SOCKSv5, 0x00, '\0', 0x00, '\0'};
	int ch;
	int port;
	char *host;

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

	if (argc < 3)
		usage();

	host = strdup(argv[0]);
	port = strtol(argv[1], NULL, 0);

	request.port = port / 255 << 8;
	request.port = port % 255;

	write(WRITE_FD, &nego, sizeof nego);
	read(READ_FD, &nego_ans, sizeof nego_ans);

	if (nego_ans.method == NOT_ACC) {
		perror("No acceptable methods");
		return EXIT_FAILURE;
	}

	write(WRITE_FD, &request, sizeof request);
	read(READ_FD, &reply, 3);

	fprintf(stderr, "reply: '%s'\n", rep_mesg(reply.rep));
	fprintf(stderr, "exit!\n");

	return EXIT_SUCCESS;
}
