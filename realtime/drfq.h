/* cs194-24 Lab 2 */

#ifndef DRFQ_H
#define DRFQ_H

#include "drf.h"

/* An opaque type that describes DRF work queues. */
typedef void * drfq_t;

/*
 * DRF Queues can operate in two modes:
 *
 * _SINGLE      Only a single thread must process an token for it
 *              to be finished.  When every token has been committed
 *              and every thread in the associated DRF instance is
 *              waiting on an _request() call, then _request() will
 *              return '-1' to every waiting thread and then reset the
 *              queue.  If a thread is killed while holding a token
 *              then that token must be reset such that it can be
 *              passed out to another thread in the same DRF instance.
 *
 * _ALL         Every thread in the DRF instance needs to commit a token 
 *              before any thread can proceed to processing the next
 *              token in the queue.  In this mode, all tokens must be
 *              passed out strictly in order.  When every thread has
 *              processed every token in the queue and every thread is
 *              waiting inside _request(), then _request will return
 *              '-1' to every waiting thread and then reset the queue.
 *              When a new thread is created by the associated DRF
 *              instance then it will be marked as having every token
 *              processed, and when a thread is killed it will be
 *              marked as having every token processed.
 */
enum drfq_mode
{
    DRFQ_MODE_INIT,
    DRFQ_MODE_SINGLE,
    DRFQ_MODE_ALL,
};

int drfq_init(drfq_t *queue);

/*
 * Creates a new DRF queue.
 *
 * queue        An opaque object that uniquely identifies this queue.
 *
 * drf          The DRF instance associated with this queue.
 *
 * max_entry    When created this queue will consist of 'max_entry'
 *              tokens, each numbered from 0 to 'max_entry-1'.
 *
 * mode         The mode of operation for this queue, see above.
 *
 * return       0 on success, negative on failure
 */
int drfq_create(drfq_t *queue, drf_t *drf,
		int max_entry, enum drfq_mode mode);

/* Requests a new DRF token from a work queue.  This returns "-1" when
 * there are no tokens left in a queue.  It's important to note that
 * tokens must be returned in order.  */
int drfq_request(drfq_t *queue);

/* Commits a new DRF token to a work queue.  When a token is committed
 * that signifies that the thread has finished doing the work
 * associated with that token. */
int drfq_commit(drfq_t *queue, int token);

#endif
