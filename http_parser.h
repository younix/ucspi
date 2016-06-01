#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

struct http_response {
	int code;
	size_t content_lenght;
	int encoding;
#	define HTTP_ENCODING_PLAIN    0x00
#	define HTTP_ENCODING_COMPRESS 0x01
#	define HTTP_ENCODING_DEFLATE  0x02
#	define HTTP_ENCODING_GZIP     0x04
#	define HTTP_ENCODING_CHUNKED  0x08
};

int http_read_line_fd(int fd, char *buf, size_t size);
int http_read_line_fh(FILE *fh, char *buf, size_t size);
int http_parse_code(char *buf, size_t size);
int http_parse_line(struct http_response *head, char *buf, size_t size);
char *http_reason_phrase(int code);

#endif
