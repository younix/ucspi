/*
 * Copyright (c) 2016 Jan Klemkow <j.klemkow@wemelug.de>
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

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

struct http_response {
	int code;
	size_t content_length;

	enum {
		HTTP_CONT_ENC_PLAIN = 0,
		HTTP_CONT_ENC_COMPRESS,
		HTTP_CONT_ENC_DEFLATE,
		HTTP_CONT_ENC_GZIP
	} content_encoding;

	enum {
		HTTP_TRANS_ENC_NONE = 0,
		HTTP_TRANS_ENC_CHUNKED
	} transfer_encoding;
};

int http_read_line_fd(int fd, char *buf, size_t size);
int http_read_line_fh(FILE *fh, char *buf, size_t size);
int http_parse_code(char *buf, size_t size);
int http_parse_line(struct http_response *head, char *buf);
char *http_reason_phrase(int code);

#endif
