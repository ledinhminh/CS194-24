// Terrible list backing structure for fd's.

#include <pthread.h>

#include "palloc.h"
#include "debug.h"

struct fd_list
{
	int fd;
	struct http_session *session;
	struct fd_list* next;
};

struct fd_list* fd_list_head;
pthread_mutex_t fd_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void fd_list_add(palloc_env* env, int fd, struct http_session* session)
{
    DEBUG("init\n");
	struct fd_list* to_add;
	struct fd_list* current;

	to_add = palloc(env, struct fd_list);
	to_add->fd = fd;
	to_add->session = session;
	to_add->next = NULL;

    DEBUG("acquiring fd_list lock\n");
	pthread_mutex_lock(&fd_list_mutex);
    DEBUG("acquired lock\n");
	if (fd_list_head == NULL)
	{
		fd_list_head = to_add;
	}
	else
	{
		current = fd_list_head;
		while (current->next != NULL)
		{
			current = current->next;
		}
		current->next = to_add;
	}
	pthread_mutex_unlock(&fd_list_mutex);
    DEBUG("released lock\n");
}

struct http_session* fd_list_find(int fd)
{
	struct fd_list* current;
	current = fd_list_head;
	while(current != NULL){
		if (current->fd == fd)
		{
			return current->session;
		}
		else
		{
			current = current->next;
		}
	}
	return NULL;
}

void fd_list_del(int fd)
{
    DEBUG("looking for fd=%d\n", fd);
	struct fd_list* current;
	struct fd_list* prev;
	struct fd_list* next;

	current = fd_list_head;
	prev = NULL;
	next = NULL;

    DEBUG("acquiring fd_list lock\n");
	pthread_mutex_lock(&fd_list_mutex);
    DEBUG("acquired lock\n");
	while(current != NULL) {
        DEBUG("current=%p, fd=%d\n", current, current->fd);
		next = current->next;
		if (current->fd == fd)
		{
			if (prev == NULL) {
				// current is head
				if (next == NULL)
					// current is the only one
					fd_list_head = NULL;
				else
					fd_list_head = next;
			} else {
				if (next == NULL)
					prev->next = NULL;
				else
					prev->next = next;
			}
            // We're done, get out.
            current = NULL;
		} else {
			prev = current;
			current = current->next;
		}
	}
	pthread_mutex_unlock(&fd_list_mutex);
    DEBUG("released lock\n");
}
