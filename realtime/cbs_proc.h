/* cs194-24 Lab 2 */

#ifndef CBS_PROC_H
#define CBS_PROC_H

/*
 * The maximum length of history that CBS needs to keep around for
 * snapshots
 */
#define CBS_MAX_HISTORY 64

/*
 * An opaque type that represents a CBS task.
 */
typedef void * cbs_proc_t;

/*
 * This needs to be a signed integral time, it represents how CBS
 * keeps time internally.
 */
typedef long cbs_time_t;

/*
 * A function that takes a CBS opaque type and some private data.
 */
typedef void (*cbs_func_t)(cbs_proc_t, void *);

/*
 * Represents a number of different interesting states that a CBS
 * process can be in.
 */
enum cbs_state
{
    CBS_STATE_HISTORY,   /* The result of a historical run */
    CBS_STATE_RUNNING,   /* The currently running process */
    CBS_STATE_READY,     /* Ready to run, in a queue somewhere */
    CBS_STATE_BLOCKED,   /* Unable to run for any reason */
    CBS_STATE_INVALID,   /* The request was not far a valid process */
};

/*
 * Enables (or disables) CBS
 *
 * This just turns on and off the scheduler, it doesn't actually clear
 * out any data structures.
 */
void cbs_enable(void);
void cbs_disable(void);

/*
 * Lists the CBS history
 *
 * This must call "func(ID, arg)" exactly once for every process
 * invocation in the history window.  The oldest history entry must
 * get called first, and the newest last.  The integer paramater
 * defines the snapshot index to iterate over.
 */
void cbs_list_history(int sid, cbs_func_t func, void *arg);

/*
 * Shows the currently running CBS process
 *
 * This is similar to _history, but it just calls with the currently
 * running process.
 */
void cbs_list_current(int sid, cbs_func_t func, void *arg);

/*
 * Shows the next CBS process that will be run.
 */
void cbs_list_next(int sid, cbs_func_t func, void *arg);

/*
 * Lists every process known to CBS except the currently running
 * process and next process to run.
 *
 * These are called in no particular order.
 */
void cbs_list_rest(int sid, cbs_func_t func, void *arg);

/*
 * Obtains the kernel-PID (a system wide unique identifier) for a process.
 */
long cbs_get_pid(cbs_proc_t p);

/*
 * Obtains the creation time of a process.
 *
 * This time is in arbitrary units, but it must be monotonically
 * increasing.  The key is that a (pid, ctime) pair is enough to
 * uniquely identify a process.
 */
cbs_time_t cbs_get_ctime(cbs_proc_t p);

/*
 * Obtains the start time of a process
 *
 * This returns the time when the process was started for entries in
 * the CBS history, and "-1" for all other entries.
 */
cbs_time_t cbs_get_start(cbs_proc_t p);

/*
 * Obtains the end time of a process
 *
 * Just like _start, this returns -1 on non-history processes
 */
cbs_time_t cbs_get_end(cbs_proc_t p);

/*
 * Obtains the period of a process
 */
cbs_time_t cbs_get_period(cbs_proc_t p);

/*
 * Obtains the compute time of a process
 *
 * The process will be allocated exactly (for real-time) or
 * approximately (for CBS) this much execution time once for every
 * period it runs.
 */
cbs_time_t cbs_get_compute(cbs_proc_t p);

/*
 * Obtains the current run state of a process.
 */
enum cbs_state cbs_get_state(cbs_proc_t p);


#endif
