#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <sys/time.h>
#include <time.h>

void mainsleep(void){
  usleep(200000);
}

void childexit(void){
  usleep(30000);
  exit(1);
}

void test0(void){
  int i = 1;

  /* Ok we are going to limit the number of child threads to 0 and ensure
   * that the main thread can start more than 0 threads
   */
  if(fork() == 0){
    /* I will spawn a thread so we do not polute the master thread */
    prctl(41, 1);
    for(i = 0; i < 5; i ++){
      int pid = fork();
      if(pid == 0){
        /* we do not need this thread much longer */
        /* lets sleep for 10 msec */
        childexit();
      } else if (pid == -1){
        /* failed  */
        fprintf(stdout, "Test0 FAILED\n");
      }
    }
    exit(1);
  }
  /* let the master thread sleep for 100 msec so the test can finish */
  mainsleep();
}

void test1(void){
  /* Lets ensure we can create threads straight down given a limit of 3 */

  if(fork() == 0){
    prctl(41, 3);
    if(fork() == 0){
      /* this child can have 3 children */
      if(fork() == 0)
        if(fork() == 0)
          if(fork() == 0){
            /* we go there so it created 3 children... should fail now */
            int npid = fork();
            if(npid == -1)
              fprintf(stdout, "Test1 PASS\n");
          }
    }
    childexit();
  }

  mainsleep();
}

void test2(void){
  int pid = 0;
  /* ensure my children can call prctl multiple times and are restricted to the
   * most strict constraint.
   */
  if(fork() == 0){
    prctl(41, 4);
    /* More Strict */
    if(fork() == 0){
      prctl(41, 1);
      /* I can have 4 children */
      /* But my children cn only have 1 */
      if(fork() == 0){
        /* I can only have 1 child */
        if(fork() == 0){
          childexit();
        }

        pid = fork();
        if(pid == -1){
          fprintf(stdout, "Test2 PASS\n");
        }
      }
      childexit();
    }
    childexit();
  }

  mainsleep();
}

void test3(void){
  /* ensure my children can call prctl multiple times and are restricted to the
   * most strict constraint.
   */

  if(fork() == 0){
    prctl(41, 1);
    if(fork() == 0){
      /* I can only have 1 child... I'm going to try and increase that */
      prctl(41, 10000);
      if(fork() == 0){
        if(fork() == -1){
          fprintf(stdout, "Test3 PASS\n");
        }
      }
    }
    childexit();
  }

  mainsleep();
}

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

  test0();
  test1();
  test2();
  test3();

  return 0;
}
