#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  int tests_ran = 0;
  printf("Lets test some prctl thread limiting!\n\n\n");
  int e = prctl(41, 0);
  if(e != -1)
    printf("ERROR: TEST 1 e should be -1, instead e = %d\n", e);
  tests_ran++;

  int limit = 2;
  e = prctl(41, limit);
  if(e != 0)
    printf("ERROR: TEST 1 e should be 0, instead e = %d\n", e);
  tests_ran++;

  printf("Ran %d tests\n", tests_ran);

  printf("Difficult to say if the rest pass or fail... Thread limit is %d\n",
         limit);
  int i = 0;
  int j = 0;
  int pid = 0;
  int s = 0;
  for(i = 0; i < 1; ++i){
    pid = fork();
    s = 2; //(rand() % 2) + 1;
    if(pid == 0){
      for(j = 0; j < 4; ++j){
        pid = fork();
        if(pid == 0){
          break;
        }
        printf("%d,%d.  Hello from pid: %d to child pid: %d\n",
               i, j, getpid(), pid);
      }
      break;
    }
    printf("%d.  Hello from pid: %d to child pid: %d\n", i, getpid(), pid);
  }

  //if(pid == 0) sleep(2);
  sleep(s);
  printf("%d.  Goodbye from pid: %d\n", i, getpid());

  return 0;
}
