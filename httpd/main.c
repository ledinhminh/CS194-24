/* cs194-24 Lab 1 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/prctl.h>

#include "http.h"
#include "mimetype.h"
#include "palloc.h"
#include "debug.h"
#include "fd_list.h"
#include "cache.h"

#define PORT 8088
#define LINE_MAX 1024
#define MAX_EVENTS 1

#define EPOLL_FLAGS EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP

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

  // Limit this threads number of children this threads children
  // can spawn to the number of cores on this system.
  // prctl(41, sysconf( _SC_NPROCESSORS_ONLN )); // Not required

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
		if (num_active_epoll < 0 && errno != EINTR) {
            DEBUG("epoll_wait failure: %s\n", strerror(errno));
			exit(1);
		}
        DEBUG("num_active_epoll=%d\n", num_active_epoll);
		for (index = 0; index < num_active_epoll; index++)
		{
			if ((events[index].events & EPOLLERR) || (events[index].events & EPOLLRDHUP))
			{
				// Something happened, the socket closed so we should close
				// it on our side
                DEBUG("epoll error\n");
				fd_list_del(events[index].data.fd);
				close(events[index].data.fd);
				continue;
			}
			else if (events[index].data.fd == socket_fd)
			{
                DEBUG("got listening socket in event\n");
				session = server->wait_for_client(server);

                DEBUG("s=%p, s->buf=%p, s->response=%p, s->buf_size=%d, s->buf_used=%d, s->done_reading=%d, s->done_processing=%d\n", session, session->buf, session->response, (int) session->buf_size, (int) session->buf_used, session->done_reading, session->done_processing);

				// Check if session is NULL
				if (session == NULL)
				{
					fprintf(stderr, "server->wait_for_client() returned NULL...\n");
					goto rearm;
				}
                DEBUG("session not null\n");

				// Try to make accepting socket non-blocking
				if (make_socket_non_blocking(session->fd) < 0)
				{
                    DEBUG("failed to set non-blocking\n");
					goto rearm;
				}
                DEBUG("socket non-blocking set\n");

				// Add session to fd_list
				fd_list_add(env, session->fd, session);
                DEBUG("added to fd_list\n");

				// Add the accepted socket into epoll
				struct epoll_event sess_event;
				sess_event.data.fd = session->fd;
				sess_event.events = EPOLL_FLAGS;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, session->fd, &sess_event) < 0) {
                    DEBUG("couldn't add socket to epoll: %s\n", strerror(errno));
					goto rearm;
				}
                DEBUG("added session socket fd=%d to epoll\n", session->fd);

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
                DEBUG("processing request, fd=%d\n", events[index].data.fd);
                fd_list_print();
				// An accepted socket fd
				session = fd_list_find(events[index].data.fd);
				DEBUG("session=%p\n", session);
                // It happens.
                if (NULL == session) {
                    DEBUG("session null. search failed?\n");
                    exit(1);
                }

				//the session at this point can be either a socket_fd or a disk_fd, we should check
				if (events[index].data.fd == session->fd)
				{
					DEBUG("handling network fd\n");

					//a socket fd, we should read from it if we have to
					ssize_t readed = 0;
					if (session->done_req_read == 0)
					{
						while (
							(readed = read(session->fd, session->buf + session->buf_used,
								session->buf_size - session->buf_used)) > 0)
						{
							DEBUG("read %d bytes, buf=(%d bytes)\n", (int) readed, (int) strlen(session->buf));
						    if (readed > 0)
						      session->buf_used += readed;

						    if (session->buf_used >= session->buf_size)
						    {
						      session->buf_size *= 2;
						      session->buf = prealloc(session->buf, session->buf_size);
						    }
						}
						if (0 == strcmp(session->buf + session->buf_used - 4, "\r\n\r\n"))
						{
							DEBUG("done reading\n");

                            session->done_req_read = 1;

							//start processing request
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

/* 							fprintf(stderr, "[%04lu] < '%s' '%s' '%s'\n", strlen(line),
								method, file, version);
 */
							headers = palloc(session, struct http_header);
							DEBUG("new http_headers at %p\n", headers);
							session->headers = headers;

							while ((line = session->gets(session)) != NULL)
							{
								size_t len;

								len = strlen(line);
								// fprintf(stderr, "[%04lu] < %s\n", len, line);
								headers->header = line;
								next_header = palloc(session, struct http_header);
								headers->next = next_header;
								headers = next_header;
	                            headers->header = NULL;

								if (len == 0)
									break;
							}
							mt = mimetype_new(session, file);
							if (strcasecmp(method, "GET") == 0)
								mterr = mt->http_get(mt, session, epoll_fd);
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
						}
						else if (readed == -1 && errno == EAGAIN)
						{
							//We got an EGAIN from reading from socket into buffer
							//We need to rearm
							DEBUG("REACHED EGAIN ARMING IN EPOLL\n");
							struct epoll_event event;
							event.data.fd = session->fd;
							event.events = EPOLL_FLAGS;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, session->fd, &event))
							{
								perror("EPOLL MOD FAIL\n\n");
							}
							DEBUG("ARMED IN EPOLL, CONTINUING\n");
							continue;
						} else {
                            session->done_req_read = 1;
                        }
					}
					else
					{
						//we've already finished reading off the socket
						mt = mimetype_new(session, file);
						if (strcasecmp(method, "GET") == 0)
							mterr = mt->http_get(mt, session, epoll_fd);
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
					}

				}

				cleanup:
                DEBUG("finished processing request\n");
			}
		}
	}
}

int main(int argc, char **argv)
{
	palloc_env env;

    struct server_thread_args thread_args;
	int socket_fd;
	int epoll_fd;
	int proc_num;
	struct epoll_event event;

	env = palloc_init("httpd root context");
    cache_init(env);

	proc_num = sysconf( _SC_NPROCESSORS_ONLN );

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
	DEBUG("creating new epoll instance\n");
	epoll_fd = epoll_create1(0);
	if(epoll_fd < 0)
	{
		perror("Couldn't create new epoll");
		return -1;
	}

	//Add the listening socket into epoll
	event.data.fd = socket_fd;
	event.events = EPOLL_FLAGS;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) < 0)
	{
		perror("Couldn't add socket to epoll");
		return -1;
	}

	// Populate the thread arguments
	// Each thread needs information so we pass it to them in a struct
	thread_args.listen_socket_fd = socket_fd;
	thread_args.thread_env = &env;
	thread_args.epoll_fd = epoll_fd;

	DEBUG("passing listening socket fd=%d to threads\n", socket_fd);

	//create array of pthreads
	pthread_t *threads;
	int i;
	threads = palloc_array(env, pthread_t, proc_num);
	for (i = 0; i < proc_num; i++){
		pthread_t thread;
		pthread_create(&thread, NULL, start_thread, (void*) &thread_args);
		threads[i] = thread;
	}

	for (i = 0; i < proc_num; i++){
		pthread_join(threads[i], NULL);
	}

	return 0;
}
