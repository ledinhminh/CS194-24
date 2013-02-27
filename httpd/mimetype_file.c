/* cs194-24 Lab 1 */

#include "palloc.h"
#include "mimetype_file.h"
#include "debug.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define BUF_COUNT 4096
#define IF_NONE_MATCH "If-None-Match: "

static int http_get(struct mimetype *mt, struct http_session *s, int epoll_fd);

struct mimetype *mimetype_file_new(palloc_env env, const char *fullpath)
{
    struct mimetype_file *mtf;

    mtf = palloc(env, struct mimetype_file);
    if (mtf == NULL)
	return NULL;

    mimetype_init(&(mtf->mimetype));

    mtf->http_get = &http_get;
    mtf->fullpath = palloc_strdup(mtf, fullpath);

    return &(mtf->mimetype);
}

int http_get(struct mimetype *mt, struct http_session *s, int epoll_fd)
{
    if (s->done_reading)
        goto write;
    if (s->done_processing)
        goto read;


    struct mimetype_file *mtf;
    int fd;
    ssize_t readed;
    char* query_string;
    char* real_path;
    int real_path_len; 
    
    struct stat file_info;
    struct tm* timeinfo;
    char* time_repr;
    char* etag_string;
    char* date_string;
    char strftime_buffer[1024];
    struct http_header* next_header;
    int etag_matches;

    mtf = palloc_cast(mt, struct mimetype_file);
    if (mtf == NULL)
	return -1;
    
    // Attempt to deal with query strings.
    // We can improve this later...
    query_string = strstr(mtf->fullpath, "?");
    real_path_len = NULL != query_string ? query_string - mtf->fullpath : strlen(mtf->fullpath);
    real_path = palloc_array(s, char, real_path_len + 1);
    strncpy(real_path, mtf->fullpath, real_path_len);
    *(real_path + real_path_len) = '\0';
    INFO; printf("fullpath=%s, real_path_len=%d, real_path=%s\n", mtf->fullpath, real_path_len, real_path);    
    INFO; printf("query string: %s\n", query_string);
    
    // We'll just use the st_mtime for ETag.
    stat(real_path, &file_info);
    timeinfo = gmtime(&file_info.st_mtime);
    psnprintf(time_repr, s, "\"%d\"", (int) file_info.st_mtime);
    psnprintf(etag_string, s, "ETag: %s\r\n", time_repr);
    strftime(strftime_buffer, 1024, "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
    psnprintf(date_string, s, "Date: %s\r\n", strftime_buffer);
    
    // Time to actually check the ETag.
    next_header = s->headers;
    do  {
        // If we overshot, go home
        if (NULL == next_header->header)
            break;

        // This is the headewr we want. Success
        if (NULL != strstr(next_header->header, IF_NONE_MATCH)) {
            etag_matches = strcmp(next_header->header + strlen(IF_NONE_MATCH), time_repr);
        }
    } while (NULL != (next_header = next_header->next));
    
    char* response;
    // ETag matched in header processing. Issue a 304 and return
    if (0 == etag_matches) {
        DEBUG("ETag matched, retuning 304\n");
        psnprintf(response, s, "%s%s%s%s",
            "HTTP/1.1 304 Not Modified\r\n",
            etag_string,
            date_string,
            "\r\n");
    } else {
        psnprintf(response, s, "%s%s%s%s%s",
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/html\r\n",
            "Cache-Control: max-age=365, public\r\n",
            etag_string,
            "\r\n");
    }
    s->response = response;
    s->done_processing = 1;
    
    fd = open(real_path, O_RDONLY);
    s->disk_fd = fd;

    read:

    while ((readed = read(s->disk_fd, s->buf, s->buf_size - s->buf_used)) >= 0)
    {
        s->buf_used += readed;

        if (s->buf_used >= s->buf_size)
        {
            s->buf_size *= 2;
            s->buf = prealloc(s->buf, s->buf_size);
        }
    }
    *(s->buf + s->buf_used + 1) = '\0';
    psnprintf(s->response, s, "%s%s", s->response, s->buf);
    s->done_reading = 1;

 
    write:
    ;
    size_t written;

    ssize_t response_length;
 
    written = 0;
    response_length = strlen(s->response);
    while ((written = s->write(s, s->response + s->buf_used, response_length - s->buf_used)) > 0)
    {
        s->buf_used += written;
    }
    if (written == -1 && errno == EAGAIN)
    {
        struct epoll_event event;
        event.data.fd = s->fd;
        event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, s->fd, &event) < 0)
        {
            perror("Couldn't arm socked fd to epoll");
        }
    }
    else if(written == 0)
    {
        close(s->fd);
    }

    return 0;
}
