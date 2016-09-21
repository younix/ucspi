/*
 * Copyright (c) 2016 Jan Klemkow <j.klemkow@wemelug.de>
 * Copyright (c) 2016 Alexander Bluhm <bluhm@openbsd.org>
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

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int *s;
	size_t slen = 1;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof addr;
	const char *errstr = NULL;

	if (argc > 1) {
		slen = strtonum(argv[1], 1, UINT16_MAX, &errstr);
		if (errstr != NULL)
			err(EXIT_FAILURE, "strtonum");
	}

	if ((s = calloc(slen, sizeof *s)) == NULL)
		err(EXIT_FAILURE, "calloc");

	for (size_t i = 0; i < slen; i++) {
		if ((s[i] = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			err(EXIT_FAILURE, "socket");

		memset(&addr, 0, sizeof addr);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = 0;

		if (bind(s[i], (struct sockaddr *)&addr, addrlen) == -1)
			err(EXIT_FAILURE, "bind");

		if (getsockname(s[i], (struct sockaddr *)&addr, &addrlen) == -1)
			err(EXIT_FAILURE, "getsockname");

		printf("%u\n", ntohs(addr.sin_port));
	}

	for (size_t i = 0; i < slen; i++)
		if (close(s[i]) == -1)
			err(EXIT_FAILURE, "close");

	return EXIT_SUCCESS;
}
