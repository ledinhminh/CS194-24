/* cs194-24 Lab 2 */

#define _XOPEN_SOURCE
#define _BSD_SOURCE
#define _POSIX_C_SOURCE 199309L

#include "cbs.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <syscall.h>

#ifndef BOGO_MIPS
#warning "BOGO_MIPS isn't defined, it should be by the build system"
#endif

#define NANO (1000 * 1000 * 1000)
#define MICRO (1000 * 1000)

/* FIXME: This is defined in <bits/sigevent.h>, but it doesn't seem to
 * actually be getting used... does anyone know why? */
#define sigev_notify_thread_id _sigev_un._tid

enum cbs_state
{
    CBS_STATE_SLEEP,
    CBS_STATE_RUNNING,
    CBS_STATE_READY,
    CBS_STATE_DONE,
};

struct cbs_struct
{
    pthread_t wt, st;

    struct timeval period;
    size_t cpu;
    enum cbs_type type;

    int (*entry)(void *);
    void *arg;

    enum cbs_state run;
    pthread_mutex_t run_lock;
    pthread_cond_t run_signal;

    int ret;
};

static void *pthread_wrapper(void *arg)
{
    struct cbs_struct *cs;
    timer_t timer;
    struct sigevent se;
    struct itimerspec its, itsoff;
    double mi_frac;
    cs = arg;

    memset(&se, 0, sizeof(se));
    se.sigev_notify = SIGEV_THREAD_ID;
    se.sigev_signo = SIGXCPU;
    se.sigev_notify_thread_id = syscall(__NR_gettid);
    if (timer_create(CLOCK_MONOTONIC, &se, &timer) != 0)
	abort();

    mi_frac = cs->cpu / BOGO_MIPS;
    memset(&its, 0, sizeof(its));
    if (cs->type == CBS_RT)
    {
	its.it_value.tv_sec = mi_frac;
	its.it_value.tv_nsec = (long)(mi_frac * NANO) % NANO;

	if (its.it_value.tv_sec > cs->period.tv_sec)
	    its.it_value.tv_sec = cs->period.tv_sec;
	if (its.it_value.tv_nsec > cs->period.tv_usec * 1000)
	    its.it_value.tv_nsec = cs->period.tv_usec * 1000;
    }

    memset(&itsoff, 0, sizeof(itsoff));

    cs->ret = 0;
    while (cs->ret == 0)
    {
	pthread_mutex_lock(&cs->run_lock);
	while (cs->run != CBS_STATE_READY)
	    pthread_cond_wait(&cs->run_signal, &cs->run_lock);
	
	cs->run = CBS_STATE_RUNNING;
	pthread_mutex_unlock(&cs->run_lock);

	if (timer_settime(timer, 0, &its, NULL) != 0)
	    abort();

	cs->ret = cs->entry(cs->arg);

	if (timer_settime(timer, 0, &itsoff, NULL) != 0)
	    abort();

	pthread_mutex_lock(&cs->run_lock);
	cs->run = CBS_STATE_SLEEP;
	pthread_mutex_unlock(&cs->run_lock);
    }

    pthread_mutex_lock(&cs->run_lock);
    cs->run = CBS_STATE_DONE;
    pthread_mutex_unlock(&cs->run_lock);

    return NULL;
}

static void *pthread_sleeper(void *arg)
{
    struct cbs_struct *cs;
    cs = arg;

    pthread_mutex_lock(&cs->run_lock);
    while (cs->run != CBS_STATE_DONE)
    {
	pthread_mutex_unlock(&cs->run_lock);

	if (cs->period.tv_sec != 0)
	    sleep(cs->period.tv_sec);
	usleep(cs->period.tv_usec);

	pthread_mutex_lock(&cs->run_lock);
	if (cs->run == CBS_STATE_SLEEP)
	    cs->run = CBS_STATE_READY;

	pthread_cond_signal(&cs->run_signal);
	pthread_mutex_unlock(&cs->run_lock);
    }

    cs->run = CBS_STATE_DONE;
    pthread_mutex_unlock(&cs->run_lock);

    return NULL;
}

int cbs_create(cbs_t *thread, enum cbs_type type,
	       size_t cpu, struct timeval *period,
               int (*entry)(void *), void *arg)
{
    struct cbs_struct *cs;

    *thread = NULL;

    cs = malloc(sizeof(*cs));
    if (cs == NULL)
	return -1;

    cs->entry = entry;
    cs->arg = arg;
    cs->period = *period;
    cs->cpu = cpu;
    cs->type = type;

    cs->run = CBS_STATE_SLEEP;

    if (pthread_mutex_init(&cs->run_lock, NULL) != 0)
	abort();

    if (pthread_cond_init(&cs->run_signal, NULL) != 0)
	abort();

    if (pthread_create(&cs->wt, NULL, &pthread_wrapper, cs) != 0)
	abort();

    if (pthread_create(&cs->st, NULL, &pthread_sleeper, cs) != 0)
	abort();

    *thread = cs;

    return 0;
}

int cbs_join(cbs_t *thread, int *code)
{
    struct cbs_struct *cs;
    cs = *thread;

    pthread_join(cs->wt, NULL);
    pthread_join(cs->st, NULL);

    return cs->ret;
}
