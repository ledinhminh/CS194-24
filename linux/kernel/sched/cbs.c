#include <linux/latencytop.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/profile.h>
#include <linux/interrupt.h>

#include <trace/events/sched.h>

#include "sched.h"

static inline int on_cbs_rq(struct sched_cbs_entity *cbs_se);
static void enqueue_task_cbs(struct rq *rq, struct task_struct *p, int flags);
static void dequeue_task_cbs(struct rq *rq, struct task_struct *p, int flags);
static struct task_struct *pick_next_task_cbs(struct rq *rq);
static void put_prev_task_cbs(struct rq* rq, struct task_struct* p);
static void check_preempt_curr_cbs(struct rq *rq, struct task_struct *p, int flags);
static void task_tick_cbs(struct rq *rq, struct task_struct *p, int queued);
static void set_curr_task_cbs(struct rq *rq);
static void switched_to_cbs(struct rq *rq, struct task_struct *p);
static void update_curr_cbs(struct rq* rq);
static inline int entity_before_cbs(struct sched_cbs_entity *a, struct sched_cbs_entity *b);
static void __enqueue_entity_cbs(struct cbs_rq* rq, struct sched_cbs_entity* cbs_se);
static void __dequeue_entity_cbs(struct cbs_rq* cbs_rq, struct sched_cbs_entity* cbs_se);
struct sched_cbs_entity* __pick_first_entity_cbs(struct cbs_rq* cbs_rq);
static struct sched_cbs_entity* __pick_next_entity_cbs(struct sched_cbs_entity* cbs_se);
struct sched_cbs_entity* __pick_last_entity_cbs(struct cbs_rq* cbs_rq);
void init_cbs_rq(struct cbs_rq *cbs_rq);

/* Some container checking functions. */

static inline int on_cbs_rq(struct sched_cbs_entity *cbs_se)
{
	return !RB_EMPTY_NODE(&cbs_se->run_node);
}

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
    // trace_printk("cbs: pick_next_task called\n");
    return NULL;
    struct sched_cbs_entity* cbs_se = __pick_first_entity_cbs(&rq->cbs);
    struct task_struct* p = container_of(cbs_se, struct task_struct, cbs);

    // Set the start time to now.
    p->se.exec_start = rq->clock_task;
    return NULL;
    // return p;
}

/* What does put_prev_task do?
 * Via http://permalink.gmane.org/gmane.linux.kernel.kernelnewbies/37656:
 *   put_prev_task first announces to the scheduler class that the currently running
 *   task is going to be replaced by another one.
 *
 *   So I guess, that prev was preempted but it is still in the running queue. Now
 *   the scheduler has to dequeue it, and to schedule another task.
 *
 * If you look at put_prev_task_rt, it just updates everyone's stats (update_curr_rt)
 *   ** WHERE  A TASK MIGHT GET RESCHEDULED IF IT HAS EXCEEDED ITS RR QUANTUM **
 * and then puts the task back on the rt_rq.
 *
 * Meanwhile, CFS just puts it back on the cfs_rq.
 *
 * So I think what we should do is just update stats and put it back on the queue.
 */
static void put_prev_task_cbs(struct rq* rq, struct task_struct* p)
{
    update_curr_cbs(rq);
    
    /* It's entirely possible that the previous task isn't on CBS, in which case
     * we should make no attempt to place it on our runqueue again. 
     * No, I don't know what p->nr_cpus_allowed is either. Ask rt.c.
     */
    if (on_cbs_rq(&p->cbs) && p->nr_cpus_allowed > 1) {
        /* TODO: What is the difference between a rt_entity and a pushable_task?
         * And why does it have to task_current in enqueue_task_rt to enqueue
         * pushable task, while we just do it here? Questions, questions...
         */
        __enqueue_entity_cbs(&rq->cbs, &p->cbs);
    }
    
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

/* Not a sched_class function. But everyone else seems to abstract away their
 * task_tick business into a update_curr. So we'll do it too.
 */

static void update_curr_cbs(struct rq* rq)
{

}

/* RB-tree manupulation methods, lifted straight from CFS. Thanks CFS! */

static inline int entity_before_cbs(struct sched_cbs_entity *a,
				struct sched_cbs_entity *b)
{
	return (s64)(a->deadline - b->deadline) < 0;
}

/*
 * Enqueue an entity into the rb-tree:
 */
static void __enqueue_entity_cbs(struct cbs_rq* cbs_rq, struct sched_cbs_entity* cbs_se)
{
	struct rb_node **link = &cbs_rq->tasks_timeline.rb_node;
	struct rb_node *parent = NULL;
	struct sched_cbs_entity *entry;
	int leftmost = 1;

	/*
	 * Find the right place in the rbtree:
	 */
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct sched_cbs_entity, run_node);
		/*
		 * We dont care about collisions. Nodes with
		 * the same key stay together.
		 */
		if (entity_before_cbs(cbs_se, entry)) {
			link = &parent->rb_left;
		} else {
			link = &parent->rb_right;
			leftmost = 0;
		}
	}

	/*
	 * Maintain a cache of leftmost tree entries (it is frequently
	 * used):
	 */
	if (leftmost)
		cbs_rq->rb_leftmost = &cbs_se->run_node;

	rb_link_node(&cbs_se->run_node, parent, link);
	rb_insert_color(&cbs_se->run_node, &cbs_rq->tasks_timeline);
}

static void __dequeue_entity_cbs(struct cbs_rq* cbs_rq, struct sched_cbs_entity* cbs_se)
{
	if (cbs_rq->rb_leftmost == &cbs_se->run_node) {
		struct rb_node *next_node;

		next_node = rb_next(&cbs_se->run_node);
		cbs_rq->rb_leftmost = next_node;
	}

	rb_erase(&cbs_se->run_node, &cbs_rq->tasks_timeline);
}

struct sched_cbs_entity *__pick_first_entity_cbs(struct cbs_rq* cbs_rq)
{
	struct rb_node *left = cbs_rq->rb_leftmost;

	if (!left)
		return NULL;

	return rb_entry(left, struct sched_cbs_entity, run_node);
}

static struct sched_cbs_entity *__pick_next_entity_cbs(struct sched_cbs_entity* cbs_se)
{
	struct rb_node *next = rb_next(&cbs_se->run_node);

	if (!next)
		return NULL;

	return rb_entry(next, struct sched_cbs_entity, run_node);
}

struct sched_cbs_entity *__pick_last_entity_cbs(struct cbs_rq* cbs_rq)
{
	struct rb_node *last = rb_last(&cbs_rq->tasks_timeline);

	if (!last)
		return NULL;

	return rb_entry(last, struct sched_cbs_entity, run_node);
}


void init_cbs_rq(struct cbs_rq *cbs_rq)
{
	cbs_rq->tasks_timeline = RB_ROOT;
    printk("cbs: initialized cbs_rq\n");
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
