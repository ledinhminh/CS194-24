/* cs194-24 Lab 1 */

#ifndef MIMETYPE_H
#define MIMETYPE_H

#include "palloc.h"
#include "http.h"

struct mimetype
{
  int (*http_get)(struct mimetype*, struct http_session*, int epoll_fd);
};

void mimetype_init(struct mimetype *mt);

int write_to_socket(struct http_session *s, int epoll_fd);
int write_to_socket_c(struct http_session *s, int epoll_fd, int close_fd);

struct mimetype *mimetype_new(palloc_env env, const char *path);

#endif
