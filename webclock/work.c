/* cs194-24 Lab 1 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "work.h"

#define THREAD_COUNT 8
#define WORK_NEEDED 20

static void *worker_thread(void *);

static int work = 0;
static pthread_mutex_t work_lock = PTHREAD_MUTEX_INITIALIZER;
volatile static unsigned int r = 0;

void do_work(void)
{
    pthread_t thr[THREAD_COUNT];
    int stat[THREAD_COUNT];
    int i;
    
    for (i = 0; i < THREAD_COUNT; i++)
	stat[i] = pthread_create(&thr[i], NULL, &worker_thread, NULL);

    for (i = 0; i < THREAD_COUNT; i++)
	if (stat[i] == 0)
	    pthread_join(thr[i], NULL);
}

void *worker_thread(void *unused __attribute__((unused)))
{
    int i, j;
    int id;

    pthread_mutex_lock(&work_lock);
    id = work++;
    pthread_mutex_unlock(&work_lock);

    for (i = 0; i < 10000; i++)
	for (j = 0; j < 1000; j++)
	    r = 33 * r ^ id;

    return NULL;
}
