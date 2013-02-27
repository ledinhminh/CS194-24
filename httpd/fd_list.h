/* cs194-24 Lab 1 */

#ifndef FD_LIST_H
#define FD_LIST_H

void fd_list_add(palloc_env* env, int fd, struct http_session* session);
struct http_session* fd_list_find(int fd);
void fd_list_del(int fd);

#endif
