#include <err.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FILE *in;
FILE *out;
bool debug = false;

#define DEBUG(...)	if (debug) warnx(__VA_ARGS__)

int
reply(char **str)
{
	char line[BUFSIZ];
	char replstr[5];	/* 123- */
	int repl;		/* reply code */
	char ch;

	if (fgets(line, sizeof line, in) == NULL)
		err(EXIT_FAILURE, "fgets");
	line[strcspn(line, "\r\n")] = '\0';
	DEBUG("%s", line);

	if (sscanf(line, "%d%c", &repl, &ch) != 2)
		err(EXIT_FAILURE, "sscanf");

	if (ch == ' ')
		goto out;

	if (ch != '-')
		err(EXIT_FAILURE, "protocol error: invaild reply string");

	if (snprintf(replstr, sizeof replstr, "%d ", repl) != 5)
		err(EXIT_FAILURE, "protocol error: invaild reply string");

	do {
		if (fgets(line, sizeof line, in) == NULL)
			err(EXIT_FAILURE, "fgets");
		line[strcspn(line, "\r\n")] = '\0';
		DEBUG("%s", line);
	} while (strncmp(line, replstr, 4));
 out:
	if (str != NULL)
		*str = strdup(line);

	return repl;
}

int
vcmd(char **replstr, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(out, fmt, ap);
	va_end(ap);

	fputs("\r\n", out);

	return reply(replstr);
}

FILE *
pasv(const char *path, char dir)
{
	FILE *fh;
	char  cmd[BUFSIZ];
	char *replstr;
	unsigned char addr[4];
	unsigned char port[2];
	unsigned short serv;

	if ((vcmd(&replstr, "PASV")) != 227)
		err(EXIT_FAILURE, "PASV");
	DEBUG("pasv: %s", replstr);

	if (sscanf(replstr, "%*d %*[^(](%hhu,%hhu,%hhu,%hhu,%hhu,%hhu)",
	    &addr[0], &addr[1], &addr[2], &addr[3], &port[0], &port[1]) != 6)
		err(EXIT_FAILURE, "parsing error: %s", replstr);
	free(replstr);

	serv = port[0] << 8 | port[1];

	snprintf(cmd, sizeof cmd, "nc -N %hhu.%hhu.%hhu.%hhu %hu",
	    addr[0], addr[1], addr[2], addr[3], serv);

	if (dir == '<' || dir == '>')
		snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd),
		    " %c '%s'", dir, path);
	DEBUG("cmd: %s", cmd);

	if ((fh = popen(cmd, "r")) == NULL)
		err(EXIT_FAILURE, "pasv: %s", cmd);

	return fh;
}

void
usage(void)
{
	fprintf(stderr, "ftpc [put|get|ls] path\n");

	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char *cmd = NULL;
	char *path = NULL;
	FILE *fh;
	char  ch;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			debug = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	cmd = argv[0];

	if (argc < 2)
		path = argv[1];

	/* Open UCSPI fds as streams */
	if ((in = fdopen(6, "r")) == NULL)
		err(EXIT_FAILURE, "fdopen");
	if ((out = fdopen(7, "w")) == NULL)
		err(EXIT_FAILURE, "fdopen");

	/* Set input and output streams line buffered */
	setvbuf(in, NULL, _IOLBF, 0);
	setvbuf(out, NULL, _IOLBF, 0);

	if (reply(NULL) != 220)
		err(EXIT_FAILURE, "init error");

	/* Login */
	switch (vcmd(NULL, "USER %s", "anonymous")) {
	case 230:
		goto auth;
	case 331:
		goto pass;
	default:
		errx(EXIT_FAILURE, "user");
	}

 pass:
	if (vcmd(NULL, "PASS %s", "mail@example.com") != 230)
		err(EXIT_FAILURE, "pass");

 auth:	/* Authenticated */

	/* Set binary mode */
	if (vcmd(NULL, "TYPE I") != 200)
		errx(EXIT_FAILURE, "TYPE");


	if (strcmp(cmd, "put") == 0) {
		fh = pasv(path, '<');

		if (vcmd(NULL, "STOR %s", path) != 150)
			errx(EXIT_FAILURE, "STOR");
	} else if (strcmp(cmd, "get") == 0) {
		fh = pasv(path, '>');

		if (vcmd(NULL, "RETR %s", path) != 150)
			errx(EXIT_FAILURE, "RETR");
	} else if (strcmp(cmd, "ls") == 0) {
		char line[PATH_MAX];

		fh = pasv(path, '|');

		if (vcmd(NULL, "NLST") != 150)
			errx(EXIT_FAILURE, "RETR");

		while (fgets(line, sizeof line, fh) != NULL)
			fputs(line, stdout);
	} else {
		errx(EXIT_FAILURE, "cmd %s is not implementet", cmd);
	}

	if (pclose(fh) != 0)
		err(EXIT_FAILURE, "pclose");

	if (reply(NULL) != 226)
		errx(EXIT_FAILURE, "finish");

	return EXIT_SUCCESS;
}
