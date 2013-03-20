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
#include <sched.h>
#include <sys/syscall.h>
#include <time.h>

// System headers... ~___~
#define SCHED_CBS 24

#ifndef BOGO_MIPS
#warning "BOGO_MIPS isn't defined, it should be by the build system"
#endif

#define NANO (1000 * 1000 * 1000)
#define MICRO (1000 * 1000)

/* FIXME: This is defined in <bits/sigevent.h>, but it doesn't seem to
 * actually be getting used... does anyone know why? */
#define sigev_notify_thread_id _sigev_un._tid

struct cbs_struct
{
    pthread_t thread;

    struct timeval period;
    size_t cpu;
    enum cbs_type type;

    int (*entry)(void *);
    void *arg;

    int ret;
};

struct new_sched_param {
    /*CBS SCHEDULER STUFF*/
    unsigned long long deadline;
    unsigned long long curr_budget;
    unsigned long long init_budget;
    double utilization;
    unsigned long long period;
    int type;
    int sched_priority;
};

static void *pthread_wrapper(void *arg)
{
    //have to convert args to a cbs_struct
    struct cbs_struct *cs;
    cs = arg;

    /*gotta convert BOGOMIPs to secs and nanos*/
    struct itimerspec its;
    double mi_frac = cs->cpu / BOGO_MIPS;

    memset(&its, 0, sizeof(its)); //zero it out
    if (cs->type == CBS_RT)
    {
        its.it_value.tv_sec = mi_frac;
        its.it_value.tv_nsec = (long)(mi_frac * NANO) % NANO;

        if (its.it_value.tv_sec > cs->period.tv_sec) //if budget > period
            its.it_value.tv_sec = cs->period.tv_sec; //make budget = period
        if (its.it_value.tv_nsec > cs->period.tv_usec * 1000) //same for frac part
            its.it_value.tv_nsec = cs->period.tv_usec * 1000;
    }

    struct new_sched_param cbs_params = {
       .deadline = 0, //set by scheduler
       .curr_budget = (its.it_value.tv_sec*NANO) + its.it_value.tv_nsec, //value is in nanoseconds
       .init_budget = (its.it_value.tv_sec*NANO) + its.it_value.tv_nsec, //value is in nanoseconds
       .period = ((cs->period.tv_sec*MICRO) + cs->period.tv_usec) * 1000, //value is in nanoseconds
       .utilization = mi_frac/(cs->period.tv_sec + (cs->period.tv_usec/MICRO)), //set by scheduler, initial budget/period
       .type = cs->type, 
       .sched_priority = 1, //either change <46>kernel/sched/sched.h or do this
    };

        if((sched_setscheduler(\
        syscall(__NR_gettid), \
        SCHED_CBS, \
        ((struct sched_param *) &cbs_params))) != 0){
        perror("SET SCHED FAILED");
    }

    //check cs->ret after it gets run and reschedule if needbe
    cs->ret = cs->entry(cs->arg);
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

    if (pthread_create(&cs->thread, NULL, &pthread_wrapper, cs) != 0)
        abort();

    *thread = cs;
    return 0;
}

int cbs_join(cbs_t *thread, int *code)
{
    struct cbs_struct *cs;
    cs = *thread;

    pthread_join(cs->thread, NULL);

    return cs->ret;
}
