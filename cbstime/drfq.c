/* cs194-24 Lab 2 */

#include "drfq.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

enum drfq_state
{
    DQS_FREE,
    DQS_RUN,
    DQS_COMMIT,
};

struct drfq
{
    pthread_mutex_t lock;
    pthread_cond_t signal;

    enum drfq_mode mode;
    int max_entry;
    enum drfq_state *state;
    size_t state_alloc;

    size_t max_work_units;

    ssize_t waiting;
};

int drfq_init(drfq_t *queue)
{
    struct drfq *q;

    q = malloc(sizeof(*q));
    if (q == NULL)
	return -1;

    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->signal, NULL);
    q->mode = DRFQ_MODE_INIT;
    q->max_entry = 0;
    q->state = NULL;
    q->waiting = -1;

    *queue = q;
    return 0;
}

int drfq_create(drfq_t *queue, drf_t *drf,
		int max_entry, enum drfq_mode mode)
{
    size_t i;
    struct drfq *q;
    q = *queue;

    pthread_mutex_lock(&q->lock);

    q->mode = mode;
    q->max_entry = max_entry;
    q->max_work_units = drf_max_work_units(drf);
    q->waiting = 0;

    switch (mode)
    {
    case DRFQ_MODE_SINGLE:
	q->state_alloc = max_entry;
	break;
    case DRFQ_MODE_ALL:
	q->state_alloc = max_entry * q->max_work_units;
	break;
    case DRFQ_MODE_INIT:
	abort();
	break;
    }

    q->state = malloc(sizeof(*q->state) * q->state_alloc);
    for (i = 0; i < q->state_alloc; i++)
	q->state[i] = DQS_FREE;

    pthread_cond_broadcast(&q->signal);
    pthread_mutex_unlock(&q->lock);

    return 0;
}

int drfq_request(drfq_t *queue)
{
    struct drfq *q;
    q = *queue;

    while (true)
    {
	int i;
	int i_free, i_run, i_commit;
	int ret;

	pthread_mutex_lock(&q->lock);
	while (q->mode == DRFQ_MODE_INIT)
	    pthread_cond_wait(&q->signal, &q->lock);
	
	i_free = i_run = i_commit = -1;
	for (i = q->state_alloc-1; i >= 0; i--)
	    if (q->state[i] == DQS_FREE)
		i_free = i;
	for (i = q->state_alloc-1; i >= 0; i--)
	    if (q->state[i] == DQS_RUN)
		i_run = i;
	for (i = 0; i < q->state_alloc; i++)
	    if (q->state[i] == DQS_COMMIT)
		i_commit = i;

	if (i_commit == q->state_alloc - 1)
	{
	    if (q->waiting == 0)
		q->waiting = q->max_work_units;

	    if (--q->waiting == 0)
		for (i = 0; i < q->state_alloc; i++)
		    q->state[i] = DQS_FREE;

	    while (q->waiting != 0)
		pthread_cond_wait(&q->signal, &q->lock);

	    pthread_cond_broadcast(&q->signal);
	    pthread_mutex_unlock(&q->lock);
	    return -1;
	}

	ret = -1;
	if (i_free != -1)
	{
	    switch (q->mode)
	    {
	    case DRFQ_MODE_SINGLE:
		ret = i_free;
		break;
	    case DRFQ_MODE_ALL:
		if (i_run == -1)
		    ret = i_free / q->max_work_units;

		if (i_free / q->max_work_units == i_run / q->max_work_units)
		    ret = i_free / q->max_work_units;
		break;
	    case DRFQ_MODE_INIT:
		abort();
		break;
	    }
	}

	if (ret != -1)
	{
	    q->state[i_free] = DQS_RUN;
	    pthread_mutex_unlock(&q->lock);
	    return ret;
	}

	pthread_cond_wait(&q->signal, &q->lock);
	pthread_mutex_unlock(&q->lock);
    }
}

int drfq_commit(drfq_t *queue, int token)
{
    struct drfq *q;
    int i, i_min, i_max;
    q = *queue;

    pthread_mutex_lock(&q->lock);

    switch (q->mode)
    {
    case DRFQ_MODE_SINGLE:
	i_min = token + 0;
	i_max = token + 1;
	break;
    case DRFQ_MODE_ALL:
	i_min = (token + 0) * q->max_work_units;
	i_max = (token + 1) * q->max_work_units;
	break;
    case DRFQ_MODE_INIT:
	abort();
	break;
    }

    for (i = i_min; i < i_max; i++)
    {
	if (q->state[i] == DQS_RUN)
	{
	    q->state[i] = DQS_COMMIT;
	    break;
	}
    }

    pthread_cond_broadcast(&q->signal);
    pthread_mutex_unlock(&q->lock);

    if (i == i_max)
	return -1;

    return 0;
}
