/* cs194-24 Lab 1 */

#include "mimetype_cgi.h"
#include "debug.h"
#include "git_date.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define BUF_COUNT 4096

// Defined in main.c
#define PORT 8088

// Some CGI environment variables, I guess.
#define CGI_SERVER_SOFTWARE "cs194-24/1.0"
#define CGI_SERVER_NAME "127.0.0.1"
#define CGI_GATEWAY_INTERFACE "CGI/1.1"
#define CGI_SERVER_PROTOCOL "HTTP/1.1"
#define EXPIRES "Expires: "

static int http_get(struct mimetype *mt, struct http_session *s);

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

/*
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
*/

int http_get(struct mimetype *mt, struct http_session *s)
{
  struct mimetype_cgi *mtc;
  FILE* fp;
  int fd;
  char buf[BUF_COUNT];
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

  while ((readed = read(fd, buf, BUF_COUNT)) > 0) {
    ssize_t written;

    written = 0;
    while (written < readed) {
      ssize_t w;

      w = s->write(s, buf + written, readed - written);
      if (w > 0)
        written += w;
    }
  }

  // Finished reading from disk setup for writing to socket

  pclose(fp);
  pfree(real_path);

  return 0;
}
