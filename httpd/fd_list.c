// Terrible list backing structure for fd's.

#include <pthread.h>

#include "palloc.h"

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
	struct fd_list* to_add;
	struct fd_list* current;

	to_add = palloc(env, struct fd_list);
	to_add->fd = fd;
	to_add->session = session;
	to_add->next = NULL;

	pthread_mutex_lock( &fd_list_mutex );
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
	pthread_mutex_unlock( &fd_list_mutex );
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
	struct fd_list* current;
	struct fd_list* prev;
	struct fd_list* next;

	current = fd_list_head;
	prev = NULL;
	next = NULL;

	pthread_mutex_lock( &fd_list_mutex );
	while(current != NULL){
		next = current->next;
		if(current->fd == fd)
		{
			if(prev == NULL)
			{
				//current is head
				if(next == NULL)
				{
					//current is the only one
					fd_list_head = NULL;
				}
				else
				{
					//next should be head
					fd_list_head = next;
				}
			}
			else
			{
				if(next == NULL)
				{
					prev->next = NULL;
				}
				else
				{
					prev->next = next;
				}
			}
			// pfree(current);
		}
		else
		{
			prev = current;
			current = current->next;
		}
	}
	pthread_mutex_unlock( &fd_list_mutex );

}
