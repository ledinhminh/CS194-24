#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char **argv){
	fprintf(stderr, "calling snapshot\n");
	if(syscall(350) != 0){
		perror("damn....");
	}
	return 0;
}