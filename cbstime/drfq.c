/* cs194-24 Lab 2 */

#include "drfq.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>


enum drfq_state
{
    DQS_FREE,
    DQS_RUN,
    DQS_COMMIT,
};

struct drfq
{
	//don't need
	// ----
    pthread_mutex_t lock;
    pthread_cond_t signal;
    // ----
    enum drfq_mode mode;
    int max_entry;
    enum drfq_state *state;
    size_t state_alloc;

    size_t max_work_units;

    ssize_t waiting;

    /* Signifies if this queue is valid
     * its set to 1 when it gets created
     * and then to 0 when request returns -1 to everyone
     * that way the next time create is called, it knows to create it
	 */
	int valid;

    /* Used to lock this drfq */
    int qlock;
    pthread_t owner;

    /* Array of tokens, each contains locks for the different threads */
    struct drf_token* tokens;
};

/* The reason why I have this array of tokens which then hold locks
 * is so we don't have to do awful math to get the right lock
 */
struct drf_token{
	struct drf_lock* locks;
};

struct drf_lock{
	int state_lock;
	enum drfq_state state;
	pthread_t *owner;
};

int drfq_init(drfq_t *queue)
{
    struct drfq *q;

    q = malloc(sizeof(*q));
    if (q == NULL)
	return -1;
	
	//won't need
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->signal, NULL);
    //----

    q->mode = DRFQ_MODE_INIT;
    q->max_entry = 0;
    q->state = NULL;
    q->waiting = -1;

    q->tokens = NULL;

    *queue = q;
    return 0;
}

int drfq_create(drfq_t *queue, drf_t *drf,
		int max_entry, enum drfq_mode mode)
{
    size_t i;
    struct drfq *q;
    q = *queue;

    while (true){ //the while loop will stop when we get the lock
	    //lets try locking the queue first before we do anything
		if(__sync_bool_compare_and_swap(&(q->qlock), 0, 1)){
			//nobody has the lock, and we just locked it
			q->owner = pthread_self();
			if (q->valid == 0){
				//queue hasn't been created yet
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

				q->valid = 1; 
			} else {
				//queue has already been created
				//don't do anything, just quietly release the lock
			}

			//set the owner to -1 for consistency sake
			q->owner = -1; 
			//free the lock
			q->qlock = 0;
			return 0;
		} else {
			//its locked, we should check if the thread is still alive
			if (pthread_kill(q->owner, 0) != 0){
				//the thread isn't running anymore
				//we should try unlocking it and start over
				__sync_bool_compare_and_swap (&(q->qlock), 1, 0);
				//if it fails, it means someone else unlocked it already
			} else {
				//the thread is still running and has the lock
				//we should do the loop again just in case that thread dies
			}
		}
    }
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
