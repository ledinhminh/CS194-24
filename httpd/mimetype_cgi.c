/* cs194-24 Lab 1 */

#include "mimetype_cgi.h"
#include "debug.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

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
    FILE* fp;
    int fd;
    char buf[BUF_COUNT];
    ssize_t readed;
    char* query_string;
    char* real_path;
    int real_path_len; 

    mtc = palloc_cast(mt, struct mimetype_cgi);
    if (mtc == NULL)
	return -1;
    
    // I guess this ought to be a function of sorts. Well, screw that!
    // Attempt to deal with query strings.
    // We can improve this later...
    query_string = strstr(mtc->fullpath, "?");
    real_path_len = NULL != query_string ? query_string - mtc->fullpath : strlen(mtc->fullpath);
    real_path = palloc_array(s, char, real_path_len + 1);
    strncpy(real_path, mtc->fullpath, real_path_len);
    *(real_path + real_path_len) = '\0';
    INFO; printf("fullpath=%s, real_path_len=%d, real_path=%s\n", mtc->fullpath, real_path_len, real_path);
    
    if (NULL != query_string)
        INFO; printf("query string: %s\n", query_string);

    // No, it cannot be O_RDONLY.
    fp = popen(real_path, "r"); 
    if (NULL == fp) {
        INFO; printf("popen returned NULL; strerror: %s\n", strerror(errno));
        return -1;
    }
    fd = fileno(fp);
    INFO; printf("fp=%p, fd=%d\n", fp, fd);

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
    pclose(fp);

    return 0;
}
