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
    int sched_priority;
    unsigned long long cpu;
    unsigned long long period;
    int type;
};

static void *pthread_wrapper(void *arg)
{
    //have to convert args to a cbs_struct
    struct cbs_struct *cs;
    cs = arg;

    // Although the scheduler has access to the exit code via task_struct, 
    // it can't restart the thread from the beginning. So we take care of it.
    while (CBS_CONTINUE == (cs->ret = cs->entry(cs->arg)));
    // Actually, this is the same thread. We need a way to signal the scheduler
    // that we don't ought to be scheduled until the beginning of the next period.
    // Oh well.
    return NULL;
}

int cbs_create(cbs_t *thread, enum cbs_type type,
	       size_t cpu, struct timeval *period,
               int (*entry)(void *), void *arg)
{
    struct cbs_struct *cs;
    int ss_rv;

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
        
    /* Note: I don't understand why BOGO_MIPS is being set at build time, when 
     * the BogoMIPS in the VM is probably slower. 
     * Piazza already claims that the scheduler in kernel land ought to--
     * Hell, this calculation ought to be done in kernel space.
     * Moving it...
     */
    
    struct new_sched_param cbs_params = {
       .sched_priority = 1, // Either change <46>kernel/sched/sched.h or do this
       .cpu = cs->cpu,
       
       // Value is in microseconds
       .period = ((cs->period.tv_sec * MICRO) + cs->period.tv_usec), 
       .type = cs->type, 
    };

    fprintf(stderr, "before set: %d\n", cbs_params.sched_priority);
    fprintf(stderr, "new_sched_param size %ld\n", sizeof(struct new_sched_param));
    
    // Sometimes this fails.
    while((ss_rv = pthread_setschedparam(cs->thread, SCHED_CBS, (struct sched_param *) &cbs_params)))
        printf("failed to set scheduler: %s\n", strerror(-ss_rv));

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
