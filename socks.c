#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

/* uscpi */
#define WRITE_FD 6
#define READ_FD 7

/* negotiation fields */
#define SOCKSv5 0x05
#define RSV 0x00

/* authentication methods */
#define NO_AUTH 0x00
#define GSSAPI 	0x01	/* not supported */
#define USRPASS 0x02	/* not supported */
#define NOT_ACC 0xFF

/* address types */
#define IPv4 0x01
#define BIND 0x03
#define IPv6 0x04

/* commands */
#define CMD_CONNECT 0x01
#define CMD_BIND    0x02 /* not supported */
#define CMD_UDP_ASS 0x03 /* not supported */

struct nego {
	uint8_t ver;
	uint8_t nmethods;
	uint8_t method;
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
	union {
		uint8_t ip6[16];
		uint8_t ip4[4];
		struct {
			uint8_t len;
			char str[255];
		} name;
	} addr;
	uint16_t port;
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
	fprintf(stderr, "socks HOST PORT PROGRAM [PROGRAM ARGS...]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[], char *envp[])
{
	struct nego nego = {SOCKSv5, 1, NO_AUTH};
	struct nego_ans nego_ans = {0, 0};
	struct request request = {SOCKSv5, CMD_CONNECT, RSV, 0, {{0}}, 0};
	struct request reply = {SOCKSv5, 0, RSV, 0, {{0}}, 0};
	int ch;

	while ((ch = getopt(argc, argv, "u:p:")) != -1) {
		switch (ch) {
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 3)
		usage();

	char *host = argv[0];// argv++; argc--;
	char *port = argv[1];// argv++; argc--;
	char *prog = argv[2];// argv++; argc--;

	if (strlen(host) > 255)
		perror("hostname is too long");

	/* parsing address argument */
	if (inet_pton(AF_INET6, host, &request.addr.ip6) == 1)
		request.atyp = IPv6;
	else if (inet_pton(AF_INET, host, &request.addr.ip4) == 1)
		request.atyp = IPv4;
	else if ((memcpy(request.addr.name.str, host, strlen(host))) != NULL)
		request.atyp = BIND;
	else
		perror("could not handle address");

	/* parsing port number */
	if ((request.port = htons(strtol(port, NULL, 0))) == 0)
		perror(NULL);

	/* socks: start negotiation */
	write(WRITE_FD, &nego, sizeof nego);
	read(READ_FD, &nego_ans, sizeof nego_ans);

	if (nego_ans.method == NOT_ACC)
		perror("No acceptable authentication methods");

	/* socks: request for connection */
	write(WRITE_FD, &request, 4);

	if (request.atyp == IPv6) {
		write(WRITE_FD, &(request.addr.ip6), 16);
	} else if (request.atyp == IPv4) {
		write(WRITE_FD, &(request.addr.ip4), 4);
	} else if (request.atyp == BIND) {
		request.addr.name.len = strlen(host);
		write(WRITE_FD, &request.addr.name.len, \
		    sizeof request.addr.name.len);
		write(WRITE_FD, request.addr.name.str, request.addr.name.len);
	} else {
		perror("this should not happen");
	}

	/* socks: send requested port */
	write(WRITE_FD, &request.port, sizeof request.port);

	/* socks: start analysing reply */
	read(READ_FD, &reply, 4);

	if (reply.cmd != 0)
		perror(rep_mesg(reply.cmd));

	/* read the bind address of the reply */
	switch (reply.atyp) {
	case IPv6:
		read(READ_FD, &reply.addr.ip6, sizeof reply.addr.ip6);
		break;
	case IPv4:
		read(READ_FD, &reply.addr.ip4, sizeof reply.addr.ip4);
		break;
	case BIND:
		read(READ_FD, &reply.addr.name.len, sizeof reply.addr.name.len);
		read(READ_FD, &reply.addr.name.str, reply.addr.name.len);
		break;
	}

	/* read the port of the replay */
	read(READ_FD, &reply.port, sizeof reply.port);
	reply.port = ntohs(request.port);

	/* start client program */
	execve(prog, argv, envp);
	perror(NULL);

	return EXIT_FAILURE;
}
