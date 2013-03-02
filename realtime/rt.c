#include "cbs.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

volatile int global_work = 0;
long long int loop_count = 100 * 1000; 

static int work(void *arg __attribute__((unused)))
{
    int i, j;

    for (i = 0; i < loop_count; i++)
	for (j = 0; j < 10 * 1000; j++)
	    global_work++;

    fprintf(stderr, "Work Done!\n");

    return 0;
}

int main(int argc, char **argv)
{
    cbs_t ct;
    struct timeval period;
    int cpu;
    enum cbs_type mode;

    cpu = 1400;
    period.tv_sec = 0;
    period.tv_usec = 100 * 1000;
    mode = CBS_RT;

    if (argc > 1 && atoi(argv[1]) != 0)
	cpu = atoi(argv[1]);
    if (argc > 2 && atoi(argv[2]) != 0)
	period.tv_sec = atoi(argv[2]);
    if (argc > 3 && atoi(argv[3]) != 0)
	period.tv_usec = atoi(argv[3]);
    if (argc > 4 && strcmp(argv[4], "CBS_RT") == 0)
	mode = CBS_RT;
    if (argc > 4 && strcmp(argv[4], "CBS_BW") == 0)
	mode = CBS_BW;

    loop_count = cpu;

    fprintf(stderr, "realtime %d %lu %lu %d\n",
	    cpu, period.tv_sec, period.tv_usec, mode == CBS_RT);

    if (cbs_create(&ct, mode, cpu, &period, &work, NULL) != 0)
	return 1;
    
    if (cbs_join(&ct, NULL) != 0)
	return 1;

    return 0;
}
