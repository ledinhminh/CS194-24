/* cs194-24 Lab 1 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "http.h"
#include "mimetype.h"
#include "palloc.h"
#include "debug.h"
#include "fd_list.h"

#define PORT 8088
#define LINE_MAX 1024
#define MAX_EVENTS 16
#define FD_SOCKET 0
#define FD_READ 1
#define FD_WRITE 2

#define EPOLL_FLAGS EPOLLIN | EPOLLET | EPOLLONESHOT

struct server_thread_args
{
	int listen_socket_fd;
	int epoll_fd;
	palloc_env* thread_env;
	struct fd_list* fd_list_head;
};

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

	//i think this will go away eventually
	server = http_server_new((palloc_env*) targ->thread_env, PORT);
	server->fd = socket_fd;

    DEBUG("socket_fd=%d\n", socket_fd);

	if (server == NULL)
        DEBUG("cannot allocate server: %s\n", strerror(errno));

	while (true)
	{
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
		if (num_active_epoll < 0) {
            DEBUG("epoll_wait failure: %s\n", strerror(errno));
			exit(1);
		}
		for (index = 0; index < num_active_epoll; index++)
		{
			if ((events[index].events & EPOLLERR) || (events[index].events & EPOLLHUP))
			{
				// Something happened, the socket closed so we should close
				// it on our side
                DEBUG("epoll error\n");
				// TODO: remove fd from structure
				close(events[index].data.fd);
				continue;
			}
			else if (events[index].data.fd == socket_fd)
			{
                DEBUG("got listening socket in event\n");
				session = server->wait_for_client(server);

				//check if session is NULL
				if (session == NULL)
				{
					fprintf(stderr, "server->wait_for_client() returned NULL...\n");
					goto rearm;
				}

				// Try to make accepting socket non-blocking
				if (make_socket_non_blocking(session->fd) < 0)
				{
                    DEBUG("failed to set non-blocking\n");
					goto rearm;
				}

				// Add session to fd_list
				fd_list_add(env, session->fd, session);

				// Add the accepted socket into epoll
				struct epoll_event sess_event;
				sess_event.data.fd = session->fd;
				sess_event.events = EPOLL_FLAGS;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, session->fd, &sess_event) < 0) {
                    DEBUG("couldn't add socket to epoll: %s\n", strerror(errno));
					goto rearm;
				}

				rearm:
				// Rearm the socket
				// Not sure why, but we have to rearm the socket with the listening socket descriptor
				// with another instance with the correct event flags...
				// I guess that the event flags are cleared.
				event.data.fd = socket_fd;
				event.events = EPOLL_FLAGS;
                DEBUG("rearming\n");
				if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, events[index].data.fd, &event)) {
                    DEBUG("epoll_ctl failed: %s\n", strerror(errno));
				}
			}
			else
			{
                DEBUG("processing request\n");

				// An accepted socket fd
				session = fd_list_find(events[index].data.fd);

				//the session at this point can be either a socket_fd or a disk_fd, we should check
				if (events[index].data.fd == session->fd)
				{
					//a socket fd, we should read from it
					ssize_t readed;
					while (
						(readed = read(session->fd, session->buf + session->buf_used, 
							session->buf_size - session->buf_used)) > 0)
					{		
					    if (readed > 0)
					      session->buf_used += readed;

					    if (session->buf_used >= session->buf_size)
					    {
					      session->buf_size *= 2;
					      session->buf = prealloc(session->buf, session->buf_size);
					    }
					}

					if (readed == 0)
					{
						//start processing request
					}
					else if (readed == EAGAIN)
					{
						struct epoll_event event;
						event.data.fd = events[index].data.fd;
						event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
						if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, events[index].data.fd, &event))
						{
							perror("EPOLL MOD FAIL\n\n");
						}
						continue;
					}
				}
				else
				{
					//a disk_fd, we should read from disk
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

				headers = palloc(session, struct http_header);
				DEBUG("new http_headers at %p\n", headers);
				session->headers = headers;

				while ((line = session->gets(session)) != NULL)
				{
					size_t len;

					len = strlen(line);
					fprintf(stderr, "[%04lu] < %s\n", len, line);
					headers->header = line;
					next_header = palloc(session, struct http_header);
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
				close(session->fd);
				pfree(session);
                
                DEBUG("finished processing request\n");
				
			} //end of else if
		} //end of epoll event for
	} //end of while loop
} //end of thread_start

int main(int argc, char **argv)
{
	palloc_env env;

    struct server_thread_args thread_args;
	int debug_threaded;
	int socket_fd;
	int epoll_fd;
	struct epoll_event event;

	pthread_t thread1;
	pthread_t thread2;

	env = palloc_init("httpd root context");

	//Create a listening sockent and listen on it
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
	DEBUG(stderr, "Creating new EPOLL\n");
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

	INFO;printf("\nUSING FD %i", socket_fd);

	debug_threaded = 1; //set to 0 if you want to revert to the old server
	if (debug_threaded){
		pthread_create(&thread1, NULL, start_thread, (void*) &thread_args);
		pthread_create(&thread2, NULL, start_thread, (void*) &thread_args);
	}
	else
	{

	}

  pthread_join( thread1, NULL);
  pthread_join( thread2, NULL);

	return 0;
}
