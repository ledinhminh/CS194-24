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

static void enqueue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
    /* In some other schedulers (rt, that-which-must-not-be-named), you might see
     * a queue for "pushable tasks". That is a mechanism for migrating tasks from
     * runqueue to runqueue when one is overloaded. For now, we will not implement
     * this.
     */
     
    struct sched_cbs_entity* cbs_se = &p->cbs;
    
    printk("cbs: enqueue_task: enqueueing '%s' (%d) on %d; flags=%d\n", p->comm, task_pid_nr(p), rq->cpu, flags);
    printk("cbs: enqueue_task: deadline=%llu, init_budget=%llu\n", cbs_se->deadline, cbs_se->init_budget);
    
    __enqueue_entity_cbs(&rq->cbs, &p->cbs);
    
    // Why do we need to do this? No one knows.
    inc_nr_running(rq);
    
    printk("cbs: nr_running=%u on %d\n", rq->nr_running, rq->cpu);
    
    printk("by the way, rq->clock=%lu\n", (unsigned long) rq->clock);
}

static void dequeue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
    printk("cbs: dequeue_task: dequeueing '%s' (%d) on %d, flags=%d\n", p->comm,task_pid_nr(p), rq->cpu, flags);
    
    __dequeue_entity_cbs(&rq->cbs, &p->cbs);
    
    dec_nr_running(rq);
}

static struct task_struct *pick_next_task_cbs(struct rq *rq)
{
    struct sched_cbs_entity* cbs_se = __pick_first_entity_cbs(&rq->cbs);
    struct task_struct* p;

    // printk("cbs: pick_next_task: rq=%pr\n", rq);
    trace_printk("called\n");


    // In that-which-must-not-be-named, there is a pre-check for the nr_running
    // of the **_rq. I guess this is the same.
    
    if (NULL == cbs_se) {
        // printk("cbs: skipped...\n");
        return NULL;
    }
    

    p = container_of(cbs_se, struct task_struct, cbs);

    printk("cbs: pick_next_task: not skipped, picking '%s' (%d)\n", p->comm, task_pid_nr(p));
    
    // Set the start time to now
    p->se.exec_start = rq->clock_task;
    return p;
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
    printk("cbs: put_prev_task: letting go of '%s' (%d) from %d\n", p->comm, task_pid_nr(p), rq->cpu);
    update_curr_cbs(rq);
}

static void check_preempt_curr_cbs(struct rq *rq, struct task_struct *p, int flags)
{

}

static void _task_tick_deadline(struct rb_node* node)
{
    struct sched_cbs_entity* entity;
    
    if (RB_EMPTY_NODE(node))
        return;
    
    entity = rb_entry(node, struct sched_cbs_entity, run_node);
    entity->deadline--;
    
    _task_tick_deadline(node->rb_left);
    _task_tick_deadline(node->rb_right);
}

static void task_tick_cbs(struct rq *rq, struct task_struct *p, int queued)
{
    // A tick of budget is used, for the current task.
    p->cbs.curr_budget--;
    
    // A tick of deadline is used, for everyone.
    // This doesn't violate any red-black constraints, because we're reducing everyone.
    _task_tick_deadline(rq->cbs.tasks_timeline.rb_node);
    
    // Is the budget completely used up?
    if (0 == p->cbs.curr_budget) {
    
        /* Update the parameters only if the time is right.
         * 1) The current deadline is in the past.
         *   OR
         * 2) Using the remaining budget with the current deadline would make
         *    the entity exceed its bandwidth.
         *    = curr_period * (init_budget / init_period) < curr_budget
         * NOTE: Remove verbatim wording!
         */
        if (0 == p->cbs.deadline || \
            p->cbs.deadline * p->cbs.init_budget / p->cbs.init_period < p->cbs.deadline) {
            p->cbs.deadline += p->cbs.init_period;
            p->cbs.curr_budget = p->cbs.init_budget;
        }
            
        // Dequeue, and re-enqueue.
        dequeue_task_cbs(rq, p, 0); // Flags?
        enqueue_task_cbs(rq, p, 0);
        printk("cbs: budget deficit; new budget=%llu, deadline=%llu\n", p->cbs.curr_budget, p->cbs.deadline);
    }
    if (0 == p->cbs.curr_budget % 10) {
        printk("cbs: task_tick: budget now %llu\n", p->cbs.curr_budget);
    }
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
