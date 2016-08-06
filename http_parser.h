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
