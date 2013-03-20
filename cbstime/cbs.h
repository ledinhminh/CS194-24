/* cs194-24 Lab 2 */

#ifndef CBS_H
#define CBS_H

#include <stddef.h>
#include <time.h>
#include <sys/time.h>

#define CBS_CONTINUE 0

/* An opaque type used to uniquely identify a CBS thread. */
typedef void * cbs_t;

/* The class of real-time process. */
enum cbs_type
{
    CBS_RT,      /* A hard real-time thread of execution. */
    CBS_BW,      /* A constant bandwidth server. */
};

/*
 * Schedule a new real-time task.
 *
 * This is designed to look a whole lot like pthread_create(), but for
 * real-time threads instead of regular POSIX threads.
 *
 * thread       A pointer to an opaque CBS thread identifier.
 *
 * type         The class of process to be scheduled.
 *
 * cpu          The CPU allocation, in Bogo-MIs
 *
 * peroid       The allocation period, specificed as a timespec
 *
 * entry        The entry point for computation.  This is a bit different
 *              than a pthread entry point in that this function needs
 *              to return at the end of every computation peroid.  A
 *              return value of CBS_CONTINUE causes another
 *              computation period to be scheduled.  Any other value
 *              signifies that the computation should be aborted and
 *              the thread killed.
 *
 * arg          An argument that will be passed to entry every time it is
 *              called
 *
 * return       0 on success, negative on failure.
 *
 * EAGAIN       There were insufficient resources to schedule the request.
 */
int cbs_create(cbs_t *thread, enum cbs_type type,
	       size_t cpu, struct timeval *period,
               int (*entry)(void *), void *arg);

/*
 * Block until a real-time computation has finished
 *
 * thread       The tread to be joined
 *
 * code         The return code from the scheduled computation
 *
 * return       0 on success, negative on failure
 */
int cbs_join(cbs_t *thread, int *code);

#endif
