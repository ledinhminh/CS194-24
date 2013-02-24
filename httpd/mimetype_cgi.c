/* cs194-24 Lab 1 */

#include "mimetype_cgi.h"
#include "debug.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_COUNT 4096

static int http_get(struct mimetype *mt, struct http_session *s);

struct mimetype *mimetype_cgi_new(palloc_env env, const char *fullpath)
{
    INFO; printf("creating new mimetype_cgi\n");
    struct mimetype_cgi *mtc;

    mtc = palloc(env, struct mimetype_cgi);
    if (mtc == NULL)
	return NULL;

    mimetype_init(&(mtc->mimetype));

    mtc->http_get = &http_get;
    mtc->fullpath = palloc_strdup(mtc, fullpath);

    return &(mtc->mimetype);
}

int http_get(struct mimetype *mt, struct http_session *s)
{
    struct mimetype_cgi *mtc;
    int fd;
    char buf[BUF_COUNT];
    ssize_t readed;

    mtc = palloc_cast(mt, struct mimetype_cgi);
    if (mtc == NULL)
	return -1;

    s->puts(s, "HTTP/1.1 200 OK\r\n");
    s->puts(s, "Content-Type: text/html\r\n");
    s->puts(s, "\r\n");

    fd = open(mtc->fullpath, O_RDONLY);

    while ((readed = read(fd, buf, BUF_COUNT)) > 0)
    {
	ssize_t written;

	written = 0;
	while (written < readed)
	{
	    ssize_t w;

	    w = s->write(s, buf+written, readed-written);
	    if (w > 0)
		written += w;
	}
    }

    close(fd);

    return 0;
}
