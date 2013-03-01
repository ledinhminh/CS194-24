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
pthread_rwlock_t fd_list_rwlock = PTHREAD_RWLOCK_INITIALIZER;

void fd_list_add(palloc_env* env, int fd, struct http_session* session)
{
    DEBUG("adding fd=%d, session=%p\n", fd, session);
	struct fd_list* to_add;
	struct fd_list* current;

	to_add = palloc(env, struct fd_list);
	to_add->fd = fd;
	to_add->session = session;
	to_add->next = NULL;

	pthread_rwlock_wrlock(&fd_list_rwlock);
    DEBUG("obtained fd_list lock\n");
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
	pthread_rwlock_unlock(&fd_list_rwlock);
    DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
    DEBUG("released fd_list lock\n");
}

struct http_session* fd_list_find(int fd)
{
    DEBUG("fd=%d\n", fd);
    pthread_rwlock_rdlock(&fd_list_rwlock);
	struct fd_list* current;
	current = fd_list_head;
	while(current != NULL) {
		if (current->fd == fd)
		{
            pthread_rwlock_unlock(&fd_list_rwlock);
            DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
			return current->session;
		}
		else
		{
			current = current->next;
		}
	}
    pthread_rwlock_unlock(&fd_list_rwlock);
    DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
	return NULL;
}

void fd_list_del(int fd)
{
    DEBUG("deleting fd=%d\n", fd);
	struct fd_list* current;
	struct fd_list* prev;
	struct fd_list* next;

	prev = NULL;
	next = NULL;

    DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
    pthread_rwlock_wrlock(&fd_list_rwlock);
    DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
    DEBUG("obtained fd_list lock\n");
    current = fd_list_head;

	while(current != NULL) {
		next = current->next;
		if (current->fd == fd) {

			if (prev == NULL) {
				// current is head
                fd_list_head = next;
			} else {
                prev->next = next;
			}
           	// pthread_rwlock_unlock(&fd_list_rwlock);
            // DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
            // We're done, get out.
            current = NULL;
            // DEBUG("released fd_list lock\n");
		} else {
			prev = current;
			current = current->next;
		}
	}
    pthread_rwlock_unlock(&fd_list_rwlock);
    DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
}

void fd_list_print(void) {
    pthread_rwlock_rdlock(&fd_list_rwlock);
    struct fd_list* entry = fd_list_head;
    if (NULL == entry) {
        DEBUG("no entries in list\n");
        pthread_rwlock_unlock(&fd_list_rwlock);
        DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
        return;
    }
        
    do {
        DEBUG("entry: fd=%d response=%p\n", entry->fd, entry->session);
    } while (NULL != (entry = entry->next));
    pthread_rwlock_unlock(&fd_list_rwlock);
    DEBUG("__nr_readers=%d\n", fd_list_rwlock.__data.__nr_readers);
}