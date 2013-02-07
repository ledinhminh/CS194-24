/* cs194-24 Lab 1 */

#define _POSIX_C_SOURCE 1
#define _BSD_SOURCE

#define MAX_PENDING_CONNECTIONS 8

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "http.h"
#include "lambda.h"
#include "palloc.h"

#define DEFAULT_BUFFER_SIZE 256

struct string
{
    size_t size;
    char *data;
};

static int listen_on_port(short port);
static struct http_session *wait_for_client(struct http_server *serv);

static int close_session(struct http_session *s);

static const char *http_gets(struct http_session *s);
static ssize_t http_puts(struct http_session *s, const char *m);
static ssize_t http_write(struct http_session *s, const char *m, size_t l);

struct http_server *http_server_new(palloc_env env, short port)
{
    struct http_server *hs;

    hs = palloc(env, struct http_server);
    if (hs == NULL)
	return NULL;

    hs->wait_for_client = &wait_for_client;

    hs->fd = listen_on_port(port);

    return hs;
}

int listen_on_port(short port)
{
    int fd;
    struct sockaddr_in addr;
    socklen_t addr_len;
    int so_true;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	return -1;

    /* SO_REUSEADDR allows a socket to bind to a port while there
     * are still outstanding TCP connections there.  This is
     * extremely common when debugging a server, so we're going to
     * use it.  Note that this option shouldn't be used in
     * production, it has some security implications.  It's OK if
     * this fails, we'll just sometimes get more errors about the
     * socket being in use. */
    so_true = true;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &so_true, sizeof(so_true));

    addr_len = sizeof(addr);
    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&addr, addr_len) < 0)
    {
	perror("Unable to bind to HTTP port");
	close(fd);
	return -1;
    }

    if (listen(fd, MAX_PENDING_CONNECTIONS) < 0)
    {
	perror("Unable to listen on HTTP port");
	close(fd);
	return -1;
    }

    return fd;
}

struct http_session *wait_for_client(struct http_server *serv)
{
    struct http_session *sess;
    struct sockaddr_in addr;
    socklen_t addr_len;

    sess = palloc(serv, struct http_session);
    if (sess == NULL)
	return NULL;

    sess->gets = &http_gets;
    sess->puts = &http_puts;
    sess->write = &http_write;

    sess->buf = palloc_array(sess, char, DEFAULT_BUFFER_SIZE);
    memset(sess->buf, '\0', DEFAULT_BUFFER_SIZE);
    sess->buf_size = DEFAULT_BUFFER_SIZE;
    sess->buf_used = 0;

    /* Wait for a client to connect. */
    addr_len = sizeof(addr);
    sess->fd = accept(serv->fd, (struct sockaddr *)&addr, &addr_len);
    if (sess->fd < 0)
    {
	perror("Unable to accept on client socket");
	pfree(sess);
	return NULL;
    }

    palloc_destructor(sess, &close_session);

    return sess;
}

int close_session(struct http_session *s)
{
    if (s->fd == -1)
	return 0;

    close(s->fd);
    s->fd = -1;

    return 0;
}

const char *http_gets(struct http_session *s)
{
    while (true)
    {
	char *newline;
	ssize_t readed;

	if ((newline = strstr(s->buf, "\r\n")) != NULL)
	{
	    char *new;

	    *newline = '\0';
	    new = palloc_array(s, char, strlen(s->buf) + 1);
	    strcpy(new, s->buf);

	    memmove(s->buf, s->buf + strlen(new) + 2,
		    s->buf_size - strlen(new));
	    s->buf_used -= strlen(new);
	    s->buf[s->buf_used] = '\0';

	    return new;
	}

	readed = read(s->fd, s->buf + s->buf_used, s->buf_size - s->buf_used);
	if (readed > 0)
	    s->buf_used += readed;

	if (s->buf_used >= s->buf_size)
	{
	    s->buf_size *= 2;
	    s->buf = prealloc(s->buf, s->buf_size);
	}
    }

    return NULL;
}

ssize_t http_puts(struct http_session *s, const char *m)
{
    size_t written;

    written = 0;
    while (written < strlen(m))
    {
	ssize_t writed;

	writed = write(s->fd, m + written, strlen(m) - written);
	if (writed < 0)
	    return -1 * written;

	written += writed;
    }

    return written;
}

ssize_t http_write(struct http_session *s, const char *m, size_t l)
{
    return write(s->fd, m, l);
}
