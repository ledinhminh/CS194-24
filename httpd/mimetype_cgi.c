/* cs194-24 Lab 1 */

#include "mimetype_cgi.h"
#include "debug.h"
#include "fd_list.h"
#include "git_date.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>

#define BUF_COUNT 4096

// Defined in main.c
#define PORT 8088

// Some CGI environment variables, I guess.
#define CGI_SERVER_SOFTWARE "cs194-24/1.0"
#define CGI_SERVER_NAME "127.0.0.1"
#define CGI_GATEWAY_INTERFACE "CGI/1.1"

#define CGI_SERVER_PROTOCOL "HTTP/1.1"

#define EXPIRES "Expires: "

static int http_get(struct mimetype *mt, struct http_session *s, int epoll_fd);

struct mimetype *mimetype_cgi_new(palloc_env env, const char *fullpath) {
  DEBUG("creating new mimetype_cgi\n");
  struct mimetype_cgi *mtc;

  mtc = palloc(env, struct mimetype_cgi);
  if (mtc == NULL)
    return NULL;

  mimetype_init(&(mtc->mimetype));

  mtc->http_get = &http_get;
  mtc->fullpath = palloc_strdup(mtc, fullpath);

  return &(mtc->mimetype);
}

int is_streaming(const char* disk_buf)
{
  const char* end_header;
  end_header = strstr(disk_buf, "\r\n\r\n");
  const char* buffering = disk_buf;
  for(buffering = strstr(disk_buf, "X-Buffering:");
      buffering != disk_buf && (long)buffering < (long)end_header;
      buffering = strstr(buffering, "X-Buffering:")) {
    const char* endline = strstr(buffering, "\r\n");
    const char* streaming = strstr(buffering, "Streaming");
    DEBUG("DB:%ld, B:%ld, E:%ld, S:%ld\n",
          (long)disk_buf,
          (long)buffering,
          (long)endline,
          (long)streaming);
    if((long)streaming < (long)endline &&
       (long)streaming > (long)buffering) {
      return 1;
    }
  }
  return 0;
}

void send_data(char* disk_buf, struct http_session *s, int epoll_fd)
{
  char* temp;

  psnprintf(temp, s, "%s", disk_buf);
  s->response = temp;
  s->buf_used = 0; //offset for response
  s->buf_size = strlen(s->response);
  s->done_reading = 1;

  // No, don't ask me why this is a separate function. Dont close :)
  write_to_socket_c(s, epoll_fd, 0);
}

int http_get(struct mimetype *mt, struct http_session *s, int epoll_fd)
{
  // If we've already run the CGI, it;s in the s->response, just
  // write to the socket
  if (s->done_reading) {
    write_to_socket(s, epoll_fd);
    return 0;
  }

  (void) epoll_fd;
  struct mimetype_cgi *mtc;
  FILE* fp;
  int fd;
  // char buf[BUF_COUNT];
  ssize_t readed;
  char* query_string;
  char* real_path;
  int real_path_len;
  char* env_vars;

  mtc = palloc_cast(mt, struct mimetype_cgi);
  if (mtc == NULL)
    return -1;

  // I guess this ought to be a function of sorts. Well, screw that!
  // Attempt to deal with query strings.
  // We can improve this later...
  query_string = strstr(mtc->fullpath, "?");
  real_path_len = NULL != query_string ? query_string - mtc->fullpath : strlen(mtc->fullpath);
  real_path = palloc_array(s, char, real_path_len + 1);
  strncpy(real_path, mtc->fullpath, real_path_len);
  *(real_path + real_path_len) = '\0';
  DEBUG("fullpath=%s, real_path_len=%d, real_path=%s\n", mtc->fullpath, real_path_len, real_path);

  if (NULL == query_string) {
    // Well, we have to have something to give to the program.
    query_string = "";
  } else {
    DEBUG("query string: %s\n", query_string);
  }

  // Prepare the environment variables.
  // There has to be a better way of doing this.
  psnprintf(env_vars, s, \
            "%s='%s' %s='%s' %s='%s' %s='%s' %s='%d' %s='%s' %s='%s' %s", \
            "SERVER_SOFTWARE", CGI_SERVER_SOFTWARE, \
            "SERVER_NAME", CGI_SERVER_NAME, \
            "GATEWAY_INTERFACE", CGI_GATEWAY_INTERFACE, \
            "SERVER_PROTOCOL", CGI_SERVER_PROTOCOL, \
            "SERVER_PORT", PORT, \
            "REQUEST_METHOD", "GET", \
            "QUERY_STRING", query_string, \
            real_path
           );

  DEBUG("env_vars=%s\n", env_vars);

  // No, it cannot be O_RDONLY.
  fp = popen(env_vars, "r");
  if (NULL == fp) {
    DEBUG("popen returned NULL; strerror: %s\n", strerror(errno));
    return -1;
  }
  fd = fileno(fp);
  DEBUG("fp=%p, fd=%d\n", fp, fd);

  // Read from disk
  DEBUG("popen()'d, time to read from pipe\n");
  char *disk_buf = palloc_array(s, char, BUF_COUNT);
  size_t disk_buf_size, disk_buf_used;
  int is_it_streaming = 0;
  disk_buf_size = BUF_COUNT;
  disk_buf_used = 0;
  while((readed = read(fd, disk_buf, disk_buf_size - disk_buf_used)) > 0) {
    DEBUG("read %d bytes from pipe\n", (int) readed);
    disk_buf_used += readed;
    // Reallocate buffer if its too small
    if (disk_buf_used + 3 >= disk_buf_size) {
      disk_buf_size *= 2;
      disk_buf = prealloc(disk_buf, disk_buf_size);
    }
    is_it_streaming = is_it_streaming ? 1 : is_streaming(disk_buf);
    if(is_it_streaming) {
      DEBUG("streaming disk_buf: %s\n", disk_buf);
      send_data(disk_buf, s, epoll_fd);
      pfree(disk_buf);
      disk_buf = palloc_array(s, char, BUF_COUNT);
      disk_buf_used -= readed;
    }
  }
  // Finished reading from disk setup for writing to socket
  if(!is_it_streaming) {
    DEBUG("streaming: Closing file");
  }
  send_data(disk_buf, s, epoll_fd);

  pclose(fp);
  pfree(real_path);

  return 0;
}

/*
 * Returns -1 if we haven't finished reading
 * Return 0 if we have
 */
int write_to_socket(struct http_session *s, int epoll_fd)
{
  return write_to_socket_c(s, epoll_fd, 1);
}

int write_to_socket_c(struct http_session *s, int epoll_fd, int close_fd)
{
  int written;
  while ((written = s->write(s, s->response + s->buf_used, s->buf_size - s->buf_used)) > 0) {
    s->buf_used += written;
  }

  if (written == -1 && errno == EAGAIN) {
    // readd to epoll instance and rearm
    DEBUG("cgi write to net got EAGAIN, rearming\n");
    struct epoll_event event;
    event.data.fd = s->fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, s->fd, &event) < 0) {
      DEBUG("failed to rearm fd: %s\n", strerror(errno));
    }
    return -1;
  } else if (close_fd == 0) {
    return 0;
  } else if (written == 0) {
    // Cleanup time!
    fd_list_del(s->fd);
    close(s->fd);
    return 0;
  } else {
    DEBUG("error writing to socket: %s\nBUF: %s\n", strerror(errno), s->response);
    fd_list_del(s->fd);
    close(s->fd);
    return -1;
  }
}
