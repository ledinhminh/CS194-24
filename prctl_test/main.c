#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <sys/time.h>
#include <time.h>


int main(int argc, char **argv)
{
  char t[1024], ct[1024], rt[1024];
  struct timeval tv, ctv, rtv;
  struct tm *tm, *ctm, *rtm;

  gettimeofday(&tv, NULL);
  memcpy(&ctv, &tv, sizeof(ctv));
  memcpy(&rtv, &tv, sizeof(rtv));

  tv.tv_sec = (tv.tv_sec / 60) * 60;

  ctv.tv_sec = (ctv.tv_sec / 60) * 60 + 60;
  tm = gmtime(&tv.tv_sec);
  strftime(t, 1024, "%a, %d %b %Y %T %z", tm);
  ctm = gmtime(&ctv.tv_sec);
  strftime(ct, 1024, "%a, %d %b %Y %T %z", ctm);
  rtm = gmtime(&rtv.tv_sec);
  strftime(rt, 1024, "%a, %d %b %Y %T %z", rtm);

  fprintf(stdout,
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html\r\n"
          "Date: %s\r\n"
          "Expires: %s\r\n"
          "ETag: \"%lx\"\r\n"
          "\r\n",
          rt,
          ct,
          tv.tv_sec
    );


  int tests_ran = 0;
  printf("Lets test some prctl thread limiting!\n\n\n");

  int limit = 3;
  int e = prctl(41, limit);
  if(e != 0)
    printf("ERROR: TEST %d e should be 0, instead e = %d\n", tests_ran, e);
  tests_ran++;

  printf("Ran %d tests\n", tests_ran);

  printf("Difficult to say if the rest pass or fail... Thread limit is %d\n",
         limit);
  int i = 0;
  int j = 0;
  int pid = 0;
  int s = 1;

  pid = fork();
  if(pid == 0){
    prctl(41, 2);
    for(i = 0; i < 4; ++i){
      pid = fork();
      if(pid == 0){
        for(j = 0; j < 3; ++j){
          pid = fork();
          if(pid == 0){
            break;
          }
          if(pid > 0){
            printf("0,%d,%d.  Hello from pid: %d to child pid: %d\n",
                   i, j, getpid(), pid);
          }
        }
      }
      if(pid > 0)
        printf("0,%d.  Hello from pid: %d to child pid: %d\n", i, getpid(), pid);
    }
  }

  sleep(s);
  printf("0,%d,%d.  Goodbye from pid: %d\n", i, j, getpid());

  return 0;
}
