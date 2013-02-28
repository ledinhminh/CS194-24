/* cs194-24 Lab 1 */

#include "palloc.h"
#include "mimetype_file.h"
#include "debug.h"
#include "fd_list.h"
#include "cache.h"

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
    // We might have been interrupted by EAGAIN. Skip back to where we were if that's the case
    if (s->done_reading)
        goto write;
    if (s->done_processing)
        goto read;

    struct mimetype_file *mtf;
    int fd;

    mtf = palloc_cast(mt, struct mimetype_file);
    if (mtf == NULL)
	return -1;
    
    // Remove query strings and store them elsewhere.
    char* query_string;
    char* real_path;
    int real_path_len; 
    query_string = strstr(mtf->fullpath, "?");
    real_path_len = NULL != query_string ? query_string - mtf->fullpath : strlen(mtf->fullpath);
    real_path = palloc_array(s, char, real_path_len + 1);
    strncpy(real_path, mtf->fullpath, real_path_len);
    *(real_path + real_path_len) = '\0';
    DEBUG("fullpath=%s, real_path_len=%d, real_path=%s\n", mtf->fullpath, real_path_len, real_path);    
    DEBUG("query string: %s\n", query_string);
    
    // Set up the string for ETag and Date.
    struct stat file_info;
    struct tm* timeinfo;
    char* time_repr;
    char* etag_string;
    char* date_string;
    char strftime_buffer[1024];
    struct http_header* next_header;
    int etag_matches = -1; // strcmp returns 0 on equality
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
    
    // ETag matched in header processing. Issue a 304 and return
    if (0 == etag_matches) {
        DEBUG("ETag matched, retuning 304\n");
        psnprintf(s->response, s, "%s%s%s%s",
            "HTTP/1.1 304 Not Modified\r\n",
            etag_string,
            date_string,
            "\r\n");
        // No reading!
        goto write;
    } else {
        psnprintf(s->response, s, "%s%s%s%s%s",
            "HTTP/1.1 200 OK\r\n",
            "Content-Type: text/html\r\n",
            "Cache-Control: max-age=365, public\r\n",
            etag_string,
            "\r\n");
    }
    s->done_processing = 1;
    DEBUG("done processing, headers=(%d bytes)\n", (int) strlen(s->response));
    
    // But can we serve from cache?
    char* cache_hit;
    cache_hit = cache_lookup(s, real_path);
    
    if (NULL != cache_hit) {
        DEBUG("serving request from cache\n");
        char* temp;
        psnprintf(temp, s, "%s%s", s->response, cache_hit);
        s->response = temp;
        goto write;
    } else {
        DEBUG("request not in cache, serving normally\n");
    }
    
    fd = open(real_path, O_RDONLY);
    if (fd < 0)
        DEBUG("failed to open file: %s\n", strerror(errno));
    s->disk_fd = fd;
    DEBUG("disk_fd=%i\n", s->disk_fd);

    read:
    DEBUG("will read from file\n");
    char* disk_buf;
    ssize_t readed;
    
    ssize_t disk_buf_size = BUF_COUNT;
    ssize_t disk_buf_used = 0;
    disk_buf = palloc_array(s, char, disk_buf_size);
    DEBUG("disk_buf_used=%d, disk_buf_size=%d\n", (int) disk_buf_used, (int) disk_buf_size);
    while ((readed = read(s->disk_fd, disk_buf, disk_buf_size - disk_buf_used)) > 0)
    {
        disk_buf_used += readed;
        DEBUG("read %d bytes from file; disk_buf_used=%d, disk_buf_size=%d\n", (int) readed, (int) disk_buf_used, (int) disk_buf_size);

        if (disk_buf_used + 3 >= disk_buf_size)
        {
            disk_buf_size *= 2;
            DEBUG("reallocing to %d bytes\n", (int) disk_buf_size);
            disk_buf = prealloc(s, disk_buf_size);
        }
    }
    *(disk_buf + disk_buf_used) = '\0';

    // We have the entire string. Time to write it to cache.
    cache_add(real_path, disk_buf);

    // It's probably not safe to write into the string we're reading from...
    char* temp;
    psnprintf(temp, s, "%s%s ", s->response, disk_buf);
    s->response = temp;
    
    s->done_reading = 1;
    // Is this safe?
    *(s->response + strlen(s->response)) = '\0';
    DEBUG("done reading %d bytes into buffer: %s\n", (int) strlen(s->response), s->response);

    write:
    DEBUG("will write to net\n");
    ssize_t written;
    int response_length;
 
    written = 0;
    response_length = strlen(s->response);
    DEBUG("writing %d bytes\n", response_length); 
    while ((written = s->write(s, s->response + s->buf_used, response_length - s->buf_used)) > 0)
    {
        s->buf_used += written;
        DEBUG("wrote %d bytes\n", (int) written);
    }
    if (written == -1 && errno == EAGAIN)
    {
        DEBUG("got EAGAIN, rearming...\n");
        struct epoll_event event;
        event.data.fd = s->fd;
        event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, s->fd, &event) < 0)
        {
            DEBUG("couldn't arm socket fd to epoll: %s\n", strerror(errno));
        }
    }
    else if(written == 0)
        close(s->fd);
    else
        DEBUG("error writing to socket: %s\n", strerror(errno));
    
    fd_list_del(s->fd);

    return 0;
}
