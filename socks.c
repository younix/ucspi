/*
 * Copyright (c) 2013-2014 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

/* enviroment */
char **environ;

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

#define WRITE(fd, ptr, len) do {				\
		if (write((fd), (ptr), (len)) < (len))		\
			goto err;				\
	} while(0);

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
	fprintf(stderr, "tcpclient PROXY-HOST PROXY-PORT "
			"socks HOST PORT PROGRAM [ARGS...]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[], char *envp[])
{
	struct nego nego = {SOCKSv5, 1, NO_AUTH};
	struct nego_ans nego_ans = {0, 0};
	struct request request = {SOCKSv5, CMD_CONNECT, RSV, 0, {{0}}, 0};
	struct request reply = {SOCKSv5, 0, RSV, 0, {{0}}, 0};
	int ch, af = AF_INET6;

	environ = envp;
	while ((ch = getopt(argc, argv, "p:")) != -1) {
		switch (ch) {
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 3) usage();
	char *host = *argv; argv++; argc--;
	char *port = *argv; argv++; argc--;
	char *prog = *argv; /* argv[0] == program name */

	if (strlen(host) > 255)
		perror("socks: hostname is too long");

	/* parsing address argument */
	if (inet_pton(AF_INET6, host, &request.addr.ip6) == 1) {
		af = AF_INET6;
		request.atyp = IPv6;
	} else if (inet_pton(AF_INET, host, &request.addr.ip4) == 1) {
		af = AF_INET;
		request.atyp = IPv4;
	} else if ((memcpy(request.addr.name.str, host, strlen(host))) != NULL){
		af = AF_INET;
		request.atyp = BIND;
	} else {
		perror("could not handle address");
	}

	/* parsing port number */
	if ((request.port = htons((uint16_t)strtol(port, NULL, 0))) == 0) goto err;

	/* socks: start negotiation */
	if (write(WRITE_FD, &nego, sizeof nego) < 0) goto err;
	if (read(READ_FD, &nego_ans, sizeof nego_ans) < 0) goto err;

	if (nego_ans.method == NOT_ACC)
		perror("No acceptable authentication methods");

	/* socks: request for connection */
	if (write(WRITE_FD, &request, 4) < 0) goto err;

	if (request.atyp == IPv6) {
		write(WRITE_FD, &request.addr.ip6, 16);
	} else if (request.atyp == IPv4) {
		write(WRITE_FD, &request.addr.ip4, 4);
	} else if (request.atyp == BIND) {
		request.addr.name.len = strlen(host);
		write(WRITE_FD, &request.addr.name.len, \
		    sizeof request.addr.name.len);
		write(WRITE_FD, request.addr.name.str, request.addr.name.len);
	} else {
		perror("this should not happen");
	}

	/* socks: send requested port */
	if (write(WRITE_FD, &request.port, sizeof request.port) < 0) goto err;

	/* socks: start analysing reply */
	if (read(READ_FD, &reply, 4) < 0) goto err;

	if (reply.cmd != 0)
		perror(rep_mesg(reply.cmd));

	/* read the bind address of the reply */
	if (reply.atyp == IPv6) {
		af = AF_INET6;
		read(READ_FD, &reply.addr.ip6, sizeof reply.addr.ip6);
	} else if (reply.atyp == IPv4) {
		read(READ_FD, &reply.addr.ip4, sizeof reply.addr.ip4);
	} else if (reply.atyp ==  BIND) {
		read(READ_FD, &reply.addr.name.len, sizeof reply.addr.name.len);
		read(READ_FD, &reply.addr.name.str, reply.addr.name.len);
	} else {
		perror("socks: unknown address type in reply");
	}

	/* read the port of the replay */
	read(READ_FD, &reply.port, sizeof reply.port);

	/* set ucspi enviroment variables */
	char *tcp_remote_ip   = getenv("TCPREMOTEIP");
	char *tcp_remote_port = getenv("TCPREMOTEPORT");
	char *tcp_remote_host = getenv("TCPREMOTEHOST");
	char *tcp_local_ip    = getenv("TCPLOCALIP");
	char *tcp_local_port  = getenv("TCPLOCALPORT");
	char *tcp_local_host  = getenv("TCPLOCALHOST");

	if (tcp_remote_ip   != NULL)setenv("SOCKSREMOTEIP"  ,tcp_remote_ip  ,1);
	if (tcp_remote_port != NULL)setenv("SOCKSREMOTEPORT",tcp_remote_port,1);
	if (tcp_remote_host != NULL)setenv("SOCKSREMOTEHOST",tcp_remote_host,1);
	if (tcp_local_ip    != NULL)setenv("SOCKSLOCALIP"   ,tcp_local_ip   ,1);
	if (tcp_local_port  != NULL)setenv("SOCKSLOCALPORT" ,tcp_local_port ,1);
	if (tcp_local_host  != NULL)setenv("SOCKSLOCALHOST" ,tcp_local_host ,1);

	char tmp[BUFSIZ];
	if (request.atyp == IPv6 || request.atyp == IPv4) {
		inet_ntop(af, &request.addr, tmp, sizeof tmp);
		setenv("TCPREMOTEIP", tmp, 1);
		unsetenv("TCPREMOTEHOST");
	} else {
		setenv("TCPREMOTEHOST", host, 1);
		unsetenv("TCPREMOTEIP");
	}
	snprintf(tmp, sizeof tmp, "%d", ntohs(request.port));
	setenv("TCPREMOTEPORT", tmp, 1);

	if (reply.atyp == IPv6 || reply.atyp == IPv4) {
		inet_ntop(af, &reply.addr, tmp, sizeof tmp);
		setenv("TCPLOCALIP", tmp, 1);
		unsetenv("TCPLOCALHOST");
	} else {
		strlcpy(tmp, reply.addr.name.str, reply.addr.name.len);
		setenv("TCPLOCALHOST", tmp, 1);
		unsetenv("TCPLOCALIP");
	}
	snprintf(tmp, sizeof tmp, "%d", ntohs(reply.port));
	setenv("TCPLOCALPORT", tmp, 1);

	/* start client program */
	execve(prog, argv, environ);
 err:
	perror("socks");
	return EXIT_FAILURE;
}
