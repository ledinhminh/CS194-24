/* cs194-24 Lab 2 */

#ifndef DRF_H
#define DRF_H

#include <stddef.h>
#include <sys/time.h>

#define DRF_CONTINUE 0

/* An opaque type used to uniquely identify a DRF instance. */
typedef void * drf_t;

/* A resource usage vector that describes the resource allocation
 * required to do a single unit of work. */
struct drf_vector
{
    size_t rv_cpu;   /* Bogo-MIs */
    struct timeval rv_cpu_T; /* nanoseconds */
    size_t rv_mem;   /* Bytes of RAM */
};

/*
 * Creates a new DRF instance
 *
 * This is designed to be similar to pthread_create(), but instead of
 * providing a UNIX resource abstraction the system's resources are
 * managed by DRF.  The resource manager will create and destroy
 * threads asynchronously in order to ensure that the system is
 * running both fairly and efficiently.
 *
 * drf          A pointer to an opaque DRF identified.
 *
 * rv           The amount of resources used for a single computation.
 *
 * max_threads  The maximum number of threads that can be run at once.
 *
 * entry        The entry point, uses the same semantics as cbs_create().
 *
 *              When any thread in the DRF instance terminates then
 *              all threads in the DRF instance will be terminated.
 *
 * args         A pointer to an array of arguments.  When thread N is
 *              started, it will be passed the argument args[N].  The
 *              idea is that this argument array replaces the
 *              traditional argument pointer in pthread_create().
 *              It's necessary to have an array because an arbitrary
 *              number of threads can be scheduled by DRF.
 *
 * return       0 on success, negative on failure
 */
int drf_create(drf_t *drf, struct drf_vector *rv, size_t rv_size,
	       size_t max_work_units, int (*entry)(void *), void **args);

/*
 * Blocks until a DRF instance finishes computing.
 *
 * drf          The DRF instance to be joined.
 *
 * code         The return code from the scheduled computation
 *
 * return       0 on success, negative on failure
 */
int drf_join(drf_t *drf, int *code);

/*
 * Obtains the maximum number of threads that can possibly be
 * scheduled by this DRF instance.
 *
 * drf          The DRF instance to ask
 *
 * return       The maximum number of threads
 */
size_t drf_max_work_units(drf_t *drf);

#endif
