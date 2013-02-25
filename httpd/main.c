/* cs194-24 Lab 1 */

#include <stdbool.h>
#include <string.h>

#include "http.h"
#include "mimetype.h"
#include "palloc.h"
#include "debug.h"

#define PORT 8088
#define LINE_MAX 1024

int main(int argc, char **argv)
{
    palloc_env env;
    struct http_server *server;

    env = palloc_init("httpd root context");
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

    return 0;
}
