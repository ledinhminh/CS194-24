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
    struct sched_cbs_entity* cbs_se = &p->cbs;
    
    printk("cbs: enqueue_task: rq=%pr, p=%pr\n", rq, p);
    
    // Why do we need to do this? No one knows.
    inc_nr_running(rq);
}

static void dequeue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
    printk("cbs: dequeue_task: rq=%pr, p=%pr\n", rq, p);
    
    dec_nr_running(rq);
}

static struct task_struct *pick_next_task_cbs(struct rq *rq)
{
	return NULL;
}

static void put_prev_task_cbs(struct rq* rq, struct task_struct* p)
{
}

static void check_preempt_curr_cbs(struct rq *rq, struct task_struct *p, int flags)
{

}

static void task_tick_cbs(struct rq *rq, struct task_struct *p, int queued)
{

}

static void set_curr_task_cbs(struct rq *rq)
{

}

static void switched_to_cbs(struct rq *rq, struct task_struct *p)
{

}

void init_cbs_rq(struct cbs_rq *cbs_rq)
{
	cbs_rq->tasks_timeline = RB_ROOT;
}



const struct sched_class cbs_sched_class = {

	.next = &rt_sched_class,
	.enqueue_task = enqueue_task_cbs,
	.dequeue_task = dequeue_task_cbs,
	.set_curr_task = set_curr_task_cbs,
	.switched_to = switched_to_cbs,

	/*GH_TODO: Do we need*/
	.check_preempt_curr = check_preempt_curr_cbs,
    .put_prev_task = put_prev_task_cbs,
	.pick_next_task = pick_next_task_cbs,
	.task_tick = task_tick_cbs,
};
