#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http_parser.h"

#define strmcmp(a, b)   \
	strncmp((a), (b), strlen(b))

int
http_read_line_fd(int fd, char *buf, size_t size)
{
	size_t len = 0;
	char c;

	memset(buf, '\0', size);

	for (;;) {
		if (read(fd, &c, sizeof c) == -1)
			return -1;

		buf[len++] = c;

		if (len == size)
			return -1;

		if (strstr(buf, "\r\n"))
			break;
	}

	return 0;
}

int
http_read_line_fh(FILE *fh, char *buf, size_t size)
{
	size_t len = 0;
	char c;

	memset(buf, '\0', size);

	for (;;) {
		if ((c = fgetc(fh)) == EOF)
			return -1;

		buf[len++] = c;

		if (len == size)
			return -1;

		if (strstr(buf, "\r\n"))
			break;
	}

	return 0;
}

int
http_parse_code(char *buf, size_t size)
{
	/* HTTP/1.1 200 OK */
	int code;
	int old_errno = errno;

	if (strnlen(buf, size) < 12)
		return -1;

	errno = 0;
	code = strtol(buf + 9, NULL, 10);
	if (errno != 0)
		return -1;
	errno = old_errno;

	return code;
}

int
http_parse_line(struct http_response *head, char *buf)
{
	int old_errno = errno;

	if (strmcmp(buf, "Content-Length:") == 0)
	{
		errno = 0;
		head->content_length = strtol(buf + 16, NULL, 10);
		if (errno != 0)
			return -1;
		errno = old_errno;
	}
	else if (strmcmp(buf, "Content-Encoding:") == 0)
	{
		if (strstr(buf, "compress") != NULL)
			head->content_encoding = HTTP_CONT_ENC_COMPRESS;

		if (strstr(buf, "deflate") != NULL)
			head->content_encoding = HTTP_CONT_ENC_DEFLATE;

		if (strstr(buf, "gzip") != NULL)
			head->content_encoding = HTTP_CONT_ENC_GZIP;
	}
	else if (strmcmp(buf, "Transfer-Encoding:") == 0)
	{
		if (strstr(buf, "chunked") != NULL)
			head->transfer_encoding = HTTP_TRANS_ENC_CHUNKED;
	}

	return 0;
}

char *
http_reason_phrase(int code)
{
	switch (code) {
		case 100: return "Continue";
		case 101: return "Switching Protocols";
		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 203: return "Non-Authoritative Information";
		case 204: return "No Content";
		case 205: return "Reset Content";
		case 206: return "Partial Content";
		case 300: return "Multiple Choices";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 304: return "Not Modified";
		case 305: return "Use Proxy";
		case 307: return "Temporary Redirect";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 402: return "Payment Required";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 406: return "Not Acceptable";
		case 407: return "Proxy Authentication Required";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 410: return "Gone";
		case 411: return "Length Required";
		case 412: return "Precondition Failed";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 415: return "Unsupported Media Type";
		case 416: return "Range Not Satisfiable";
		case 417: return "Expectation Failed";
		case 426: return "Upgrade Required";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
	}
	return NULL;
}
