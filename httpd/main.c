/* cs194-24 Lab 1 */

#include <stdbool.h>
#include <string.h>

#include "http.h"
#include "mimetype.h"
#include "palloc.h"
#include "debug.h"

#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 8088
#define LINE_MAX 1024
#define MAX_EVENTS 16
#define FD_SOCKET 0
#define FD_READ 1
#define FD_WRITE 2

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

struct server_thread_args
{
	int listen_socket_fd;
	int epoll_fd;
	palloc_env* thread_env;
	struct fd_list* fd_list_head;
};

struct fd_list
{
	int fd;
	struct http_session *session;
	struct fd_list* next;
};

//FD LIST HELPERS
struct fd_list* fd_list_head;
void fd_list_add(palloc_env* env, int fd, struct http_session* session)
{
	struct fd_list* to_add;
	struct fd_list* current;

	to_add = palloc(env, struct fd_list);
	to_add->fd = fd;
	to_add->session = session;
	to_add->next = NULL;

	pthread_mutex_lock( &mutex1 );
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
	pthread_mutex_unlock( &mutex1 );
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

	pthread_mutex_lock( &mutex1 );
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
			pfree(current);
		}
		else
		{
			prev = current;
			current = current->next;
		}
	}
	pthread_mutex_unlock( &mutex1 );

}
//END of FD LIST HELPERS
static int make_socket_non_blocking (int sfd)
{
  int flags;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1)
  {
    perror ("fcntl couldn't get flags");
    return -1;
  }

  flags |= O_NONBLOCK;
  if (fcntl (sfd, F_SETFL, flags) == -1)
  {
    perror ("fcntl could set nonblock flag");
    return -1;
  }

  return 0;
}

void* start_thread(void *args)
{
	struct http_server *server;

	//epoll stuff
	struct epoll_event *events;
	struct epoll_event event;

	//argument stuff
	struct server_thread_args *targ;
	palloc_env env;
	int epoll_fd;
	int socket_fd;


	//pull stuff out of the thread arguments struct
	targ = (struct server_thread_args*) args;
	env = *(targ->thread_env);
	epoll_fd = targ->epoll_fd;
	socket_fd = targ->listen_socket_fd;

	//create epoll list to store events that need to be handled
	events = palloc_array(env, struct epoll_event, MAX_EVENTS);

	//for the accept socket event
	event.data.fd = socket_fd;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	//i think this will go away eventually
	server = http_server_new((palloc_env*) targ->thread_env, PORT);
	server->fd = socket_fd;

	fprintf(stderr, "Started the Thread\n");

	if (server == NULL)
	{
		perror("Unable to open HTTP server");
	}

	while (true)
	{
		//absolute need
		struct http_session *session;
		struct epoll_event event;

		const char *line;
		char *method, *file, *version;
		struct mimetype *mt;
		int mterr;
		struct http_header* headers;
		struct http_header* next_header;

		int num_active_epoll, index;

		num_active_epoll = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		for (index = 0; index < num_active_epoll; index++)
		{
			if ((events[index].events & EPOLLERR) ||
				(events[index].events & EPOLLHUP))
			{
				//something happened, the socket closed so we should close
				//it on our side
				fprintf(stderr, "epoll error\n");
				//TODO: remove fd from structure
				close(events[index].data.fd);
				continue;
			}
			else if (events[index].data.fd == socket_fd)
			{
				//accept the socket and create a session from it
				INFO;printf("\n\n\nTHREAD %lu LISTENED\n", (unsigned long)pthread_self() );
				session = server->wait_for_client(server);

				//check if session is NULL
				if (session == NULL)
				{
					fprintf(stderr, "server->wait_for_client() returned NULL...\n");
					goto rearm;
				}

				//try to make accepting socket non-blocking
				if(make_socket_non_blocking(session->fd) < 0)
				{
					perror("Couldn't make accepted non-blocking");
					goto rearm;
				}

				//add session to a fd_container and add it to someplace
				fd_list_add(env, session->fd, session);

				//Add the accepted socket into epoll
				event.data.fd = session->fd;
				event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, session->fd, &event) < 0)
				{
					perror("Couldn't add socket to epoll");
					goto rearm;
				}

				rearm:
				//rearm the socket
				//not sure why, but we have to rearm the socket with the listening socket descriptor
				//with another instance with the correct event flags...
				//I guess that the event flags are cleared.
				event.data.fd = socket_fd;
				event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
				INFO;printf("THREAD %lu REARM\n", (unsigned long)pthread_self() );
				if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, events[index].data.fd, &event))
				{
					perror("EPOLL MOD FAIL\n\n");
				}
			}
			else
			{
				INFO;printf("\n\n\nTHREAD %lu MANAGE ACCEPTED\n", (unsigned long)pthread_self() );
				//an accepted socket fd
				session = fd_list_find(events[index].data.fd);
				line = session->gets(session);
				if (line == NULL)
				{
					fprintf(stderr, "Client connected, but no lines could be read\n");
					goto cleanup;
				}

				//WTF IS THIS????
				method = palloc_array(session, char, strlen(line));
				file = palloc_array(session, char, strlen(line));
				version = palloc_array(session, char, strlen(line));
				if (sscanf(line, "%s %s %s", method, file, version) != 3)
				{
					fprintf(stderr, "Improper HTTP request\n");
					goto cleanup;
				}

				fprintf(stderr, "[%04lu] < '%s' '%s' '%s'\n", strlen(line),
                method, file, version);

				headers = palloc(env, struct http_header);
				INFO; printf("new http_headers at %p\n", headers);
				session->headers = headers;
				/* Skip the remainder of the lines */
        // We can't do this now -- must examine them for If-None-Match
				while ((line = session->gets(session)) != NULL)
				{
					size_t len;

					len = strlen(line);
					fprintf(stderr, "[%04lu] < %s\n", len, line);
          // pfree(line);
					headers->header = line;
					next_header = palloc(env, struct http_header);
					headers->next = next_header;
					headers = next_header;

					if (len == 0)
						break;
				}

				mt = mimetype_new(session, file);
				if (strcasecmp(method, "GET") == 0)
					mterr = mt->http_get(mt, session);
				else
				{
					fprintf(stderr, "Unknown method: '%s'\n", method);
					goto cleanup;
				}

				if (mterr != 0)
				{
					perror("unrecoverable error while processing a client");
					abort();
				}

				cleanup:
				// fd_list_del(session->fd);
				close(session->fd);
				pfree(session);

        // Now we also have to clean up the header list. What a pain
				do  {
          // If we overshot, go home
					if (NULL == next_header->header)
						continue;
          // Free the string.
					pfree(headers->header);
          // Free the node. This really should crash, as we try to access headers->next at the end of the loop. Oh well...
					pfree(headers);
				} while (NULL != (headers = headers->next));
				
				INFO;printf("\n\n\nTHREAD %lu FINISHED MANAGE\n", (unsigned long)pthread_self() );
			} //end of else if
		} //end of epoll event for
	} //end of while loop
} //end of thread_start

int main(int argc, char **argv)
{
	palloc_env env;

	// SINGLE -----
	struct http_server *server;
	// SINGLE -----

  // MULTI ------
  //server stuff
  struct server_thread_args thread_args;
	int debug_threaded;
  //network stuff
	int socket_fd;
  //epoll stuff
	int epoll_fd;
	struct epoll_event event;
  //threading stuff
	pthread_t thread1;
	pthread_t thread2;
  // MULTI ------

	//Create server environment
	fprintf(stderr, "initializing env\n");
	env = palloc_init("httpd root context");

	//Create a listening sockent and listen on it
	fprintf(stderr, "listening on port\n");
	socket_fd = listen_on_port(PORT);
	if(socket_fd < 0)
	{
		perror("Couldn't open/listen on PORT");
		return -1;
	}

	if(make_socket_non_blocking(socket_fd) < 0)
	{
		perror("Couldn't make listener non-blocking");
		return -1;
	}

	//Create a new epoll structure
	fprintf(stderr, "Creating new EPOLL\n");
	epoll_fd = epoll_create1(0);
	if(epoll_fd < 0)
	{
		perror("Couldn't create new epoll");
		return -1;
	}

	//Add the listening socket into epoll
	event.data.fd = socket_fd;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) < 0)
	{
		perror("Couldn't add socket to epoll");
		return -1;
	}

	//populate the thread arguments
	//each thread needs information so we pass it to them in a struct
	thread_args.listen_socket_fd = socket_fd;
	thread_args.thread_env = &env;
	thread_args.epoll_fd = epoll_fd;

	debug_threaded = 1; //set to 0 if you want to revert to the old server
	if (debug_threaded){
		pthread_create(&thread1, NULL, start_thread, (void*) &thread_args);
		pthread_create(&thread2, NULL, start_thread, (void*) &thread_args);
	}
	else
	{
		fprintf(stderr, "Started non thread\n");
		server = http_server_new(env, PORT);
		if (server == NULL)
		{
			perror("Unable to open HTTP server");
			return 1;
		}

		while (true)
		{
			struct http_session *session;
			const char *line;
			char *method, *file, *version;
			struct mimetype *mt;
			int mterr;
			struct http_header* headers;
			struct http_header* next_header;

			session = server->wait_for_client(server);
			if (session == NULL)
			{
				perror("server->wait_for_client() returned NULL...");
				pfree(server);
				return 1;
			}

			line = session->gets(session);
			if (line == NULL)
			{
				fprintf(stderr, "Client connected, but no lines could be read\n");
				goto cleanup;
			}

			method = palloc_array(session, char, strlen(line));
			file = palloc_array(session, char, strlen(line));
			version = palloc_array(session, char, strlen(line));
			if (sscanf(line, "%s %s %s", method, file, version) != 3)
			{
				fprintf(stderr, "Improper HTTP request\n");
				goto cleanup;
			}

			fprintf(stderr, "[%04lu] < '%s' '%s' '%s'\n", strlen(line),
              method, file, version);

			headers = palloc(env, struct http_header);
			INFO; printf("new http_headers at %p\n", headers);
			session->headers = headers;
      //Skip the remainder of the lines
      // We can't do this now -- must examine them for If-None-Match
			while ((line = session->gets(session)) != NULL)
			{
				size_t len;

				len = strlen(line);
				fprintf(stderr, "[%04lu] < %s\n", len, line);
        // pfree(line);
				headers->header = line;
				next_header = palloc(env, struct http_header);
				headers->next = next_header;
				headers = next_header;

				if (len == 0)
					break;
			}

			mt = mimetype_new(session, file);
			if (strcasecmp(method, "GET") == 0)
				mterr = mt->http_get(mt, session);
			else
			{
				fprintf(stderr, "Unknown method: '%s'\n", method);
				goto cleanup;
			}

			if (mterr != 0)
			{
				perror("unrecoverable error while processing a client");
				abort();
			}

    cleanup:
			pfree(session);

      // Now we also have to clean up the header list. What a pain
			do  {
        // If we overshot, go home
				if (NULL == next_header->header)
					continue;
        // Free the string.
				pfree(headers->header);
        // Free the node. This really should crash, as we try to access headers->next at the end of the loop. Oh well...
				pfree(headers);
			} while (NULL != (headers = headers->next));

		}
	}

  pthread_join( thread1, NULL);
  pthread_join( thread2, NULL);

	return 0;
}
