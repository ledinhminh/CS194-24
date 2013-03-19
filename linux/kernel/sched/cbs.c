#include <linux/latencytop.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/profile.h>
#include <linux/interrupt.h>

#include <trace/events/sched.h>

#include "sched.h"


/*Called whenever task enters in a runnable state*/
static void enqueue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{

}

static void dequeue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{

}

static struct task_struct *pick_next_task_cbs(struct rq *rq)
{
	return NULL;
	/* Return null to singal next sched */
}

static void check_preempt_curr_cbs(struct rq *rq, struct task_struct *p, int flags)
{

}

static void task_tick_cbs(struct rq *rq, struct task_struct *p, int queued)
{

}

const struct sched_class cbs_sched_class = {

	.next = &rt_sched_class,
	.enqueue_task = enqueue_task_cbs,
	.dequeue_task = dequeue_task_cbs,

	/*GH_TODO: Do we need*/
	.check_preempt_curr = check_preempt_curr_cbs,
	.pick_next_task = pick_next_task_cbs,
	.task_tick = task_tick_cbs,
};

