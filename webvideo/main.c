#include "drfq.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_WORK_UNITS 8

#define MAX_BLOCKS 10

enum steps
{
    STEP_READ,
    STEP_COMPUTE,
    STEP_WRITE,
    STEP_COUNT,
};

struct thread_args
{
    drfq_t *steps;
    drfq_t *blocks;
};

static void do_work(void *private, int step, int block)
{
    fprintf(stderr, "do_work(%p, %d, %d)\n", private, step, block);

    return;
}

static int thread_main(void *private)
{
    int step;
    struct thread_args *a;
    a = private;

    /* Processing a frame consists of many steps.  Each step must
     * be fully completed before going on to the next */
    while ((step = drfq_request(a->steps)) != -1)
    {
	int block;

	while ((block = drfq_request(a->blocks)) != -1)
	{
	    do_work(private, step, block);
	    
	    /* This block has been processed successfully. */
	    if (drfq_commit(a->blocks, block))
		abort();
	}
	
	/* We're done with this step, move on to the next step in
	 * the sequence. */
	if (drfq_commit(a->steps, step) == -1)
	    abort();
    }
    
    return DRF_CONTINUE;
}

int main(int argc, char **argv)
{
    drf_t d;
    drfq_t steps;
    drfq_t blocks;
    struct drf_vector resources;
    struct thread_args **args;
    int i;
    int code;

    memset(&resources, 0, sizeof(resources));
    resources.rv_cpu = 1000;
    resources.rv_cpu_T.tv_sec = 1;
    resources.rv_cpu_T.tv_usec = 0;
    resources.rv_mem = 10 * 1024 * 1024;

    args = malloc(sizeof(*args) * MAX_WORK_UNITS);
    for (i = 0; i < MAX_WORK_UNITS; i++)
    {
	args[i] = malloc(sizeof(*args[i]));
	args[i]->steps = &steps;
	args[i]->blocks = &blocks;
    }

    drfq_init(&steps);
    drfq_init(&blocks);

    drf_create(&d, &resources, sizeof(resources), MAX_WORK_UNITS,
	       &thread_main, (void **)args);

    drfq_create(&steps, &d, STEP_COUNT, DRFQ_MODE_ALL);
    drfq_create(&blocks, &d, MAX_BLOCKS, DRFQ_MODE_SINGLE);

    drf_join(&d, &code);

    return 0;
}
