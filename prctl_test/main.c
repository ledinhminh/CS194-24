#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
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
