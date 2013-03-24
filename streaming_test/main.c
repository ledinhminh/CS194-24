#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <sys/time.h>
#include <time.h>

int main(int argc, char **argv)
{
  char ct[1024], rt[1024];
  int i = 0;
  struct timeval tv, ctv, rtv;

  gettimeofday(&tv, NULL);
  memcpy(&ctv, &tv, sizeof(ctv));
  memcpy(&rtv, &tv, sizeof(rtv));

  fprintf(stdout,
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/plain\r\n"
          "X-Buffering: Streaming\r\n"
          "Date: %s\r\n"
          "Expires: %s\r\n"
          "ETag: \"%lx\"\r\n"
          "\r\n\r\n",
          rt,
          ct,
          tv.tv_sec
    );

  for(i = 0; i <= 4000; i++){
    fprintf(stdout, "%d\n, ", i);
    usleep(1000);
  }

  return 0;
}
