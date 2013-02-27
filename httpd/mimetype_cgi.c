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

// Defined in main.c
#define PORT 8088

// Some CGI environment variables, I guess.
#define CGI_SERVER_SOFTWARE "cs194-24/1.0"
#define CGI_SERVER_NAME "127.0.0.1"
#define CGI_GATEWAY_INTERFACE "CGI/1.1"

#define CGI_SERVER_PROTOCOL "HTTP/1.1"

static int http_get(struct mimetype *mt, struct http_session *s, int epoll_fd);

struct mimetype *mimetype_cgi_new(palloc_env env, const char *fullpath)
{
    DEBUG("creating new mimetype_cgi\n");
    struct mimetype_cgi *mtc;

    mtc = palloc(env, struct mimetype_cgi);
    if (mtc == NULL)
	return NULL;

    mimetype_init(&(mtc->mimetype));

    mtc->http_get = &http_get;
    mtc->fullpath = palloc_strdup(mtc, fullpath);

    return &(mtc->mimetype);
}

int http_get(struct mimetype *mt, struct http_session *s, int epoll_fd)
{
    (void) epoll_fd;
    struct mimetype_cgi *mtc;
    FILE* fp;
    int fd;
    char buf[BUF_COUNT];
    ssize_t readed;
    char* query_string;
    char* real_path;
    int real_path_len; 
    char* env_vars;
    int env_vars_len;

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
    DEBUG("fullpath=%s, real_path_len=%d, real_path=%s\n", mtc->fullpath, real_path_len, real_path);
    
    if (NULL == query_string) {
        // Well, we have to have something to give to the program.
        query_string = "";
    } else {
        DEBUG("query string: %s\n", query_string);
    }
    
    // Prepare the environment variables.
    // There has to be a better way of doing this.
    env_vars_len = snprintf(NULL, 0, \
        "%s='%s' %s='%s' %s='%s' %s='%s' %s='%d' %s='%s' %s='%s' ", \
        "SERVER_SOFTWARE", CGI_SERVER_SOFTWARE, \
        "SERVER_NAME", CGI_SERVER_NAME, \
        "GATEWAY_INTERFACE", CGI_GATEWAY_INTERFACE, \
        "SERVER_PROTOCOL", CGI_SERVER_PROTOCOL, \
        "SERVER_PORT", PORT, \
        // main.c:75
        "REQUEST_METHOD", "GET", \
        "QUERY_STRING", query_string
        ) + 1;
    env_vars = palloc_array(s, char, env_vars_len + strlen(real_path));
    snprintf(env_vars, env_vars_len, \
        "%s='%s' %s='%s' %s='%s' %s='%s' %s='%d' %s='%s' %s='%s' ", \
        "SERVER_SOFTWARE", CGI_SERVER_SOFTWARE, \
        "SERVER_NAME", CGI_SERVER_NAME, \
        "GATEWAY_INTERFACE", CGI_GATEWAY_INTERFACE, \
        "SERVER_PROTOCOL", CGI_SERVER_PROTOCOL, \
        "SERVER_PORT", PORT, \
        "REQUEST_METHOD", "GET", \
        "QUERY_STRING", query_string
        );
        
    strcpy(env_vars + env_vars_len - 1, real_path);
    DEBUG("env_vars=%s\n", env_vars);

    // No, it cannot be O_RDONLY.
    fp = popen(env_vars, "r"); 
    if (NULL == fp) {
        DEBUG("popen returned NULL; strerror: %s\n", strerror(errno));
        return -1;
    }
    fd = fileno(fp);
    DEBUG("fp=%p, fd=%d\n", fp, fd);

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
    
    pfree(real_path);

    return 0;
}
