/* cs194-24 Lab 1 */

#include "mimetype_file.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_COUNT 4096

static int http_get(struct mimetype *mt, struct http_session *s);

struct mimetype *mimetype_file_new(palloc_env env, const char *fullpath)
{
    struct mimetype_file *mtf;

    mtf = palloc(env, struct mimetype_file);
    if (mtf == NULL)
	return NULL;

    mimetype_init(&(mtf->mimetype));

    mtf->http_get = &http_get;
    mtf->fullpath = palloc_strdup(mtf, fullpath);

    return &(mtf->mimetype);
}

int http_get(struct mimetype *mt, struct http_session *s)
{
    struct mimetype_file *mtf;
    int fd;
    char buf[BUF_COUNT];
    ssize_t readed;

    mtf = palloc_cast(mt, struct mimetype_file);
    if (mtf == NULL)
	return -1;

    s->puts(s, "HTTP/1.1 200 OK\r\n");
    s->puts(s, "Content-Type: text/html\r\n");
    s->puts(s, "\r\n");

    fd = open(mtf->fullpath, O_RDONLY);

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
