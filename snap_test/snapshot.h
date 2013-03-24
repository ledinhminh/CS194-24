/* cs194-24 Lab 2 */

#ifndef SNAPSHOT_H
#define SHAPSHOT_H
#include <errno.h>

/*
 * The maximum number of snapshots triggers that can be passed to the
 * snapshot interface.
 */
#define SNAP_MAX_TRIGGERS 8

/*
 * Different subsystems can be interrogated by the snapshot subsystem.
 * Each subsystem can provide a different set of events 
 */
enum snap_event
{
    SNAP_EVENT_CBS_SCHED,     /* Triggers when the CBS scheduler
			       * context switches a task */
};

/*
 * Snapshots can be triggered on these sorts of events
 */
enum snap_trig
{
    SNAP_TRIG_BEDGE,    /* Triggers on the edge before an event starts */
    SNAP_TRIG_AEDGE,    /* Triggers on the after before an event starts */
};

/*
 * Generates a set of snapshots
 *
 * These snapshots end up in staticly allocated kernel buffers, so
 * there is a maximum number of events you can ask for at once.
 *
 * events         An array of length "n" of the events to trigger on
 *
 * device         The device ID to trigger on, used to distinguish between
 *                different devices in the same subsystem.  For CBS
 *                this indicates the processor ID.  Note that you must
 *                process snapshots in parallel: in other words, when
 *                snapshot() is called each device should mantain its
 *                own queue of (trigger, /proc/snapshot file) pairs
 *                and process them in order.
 *
 * triggers       An array of length "n" of the trigger types
 *
 * n              The length of those arrays
 *
 * return         0 on success, -1 on failure.  This call is non-blocking
 *
 * EINVAL         The kernel cannot take "n" snapshots at once.
 *
 * EAGAIN         The kernel is currently taking a snapshot.
 */
int snapshot(enum snap_event *events, int *device,
	     enum snap_trig *triggers, size_t n)
{
	if (n > SNAP_MAX_TRIGGERS){
		return EINVAL;
	} else {
		return syscall(350, events, device, triggers, n);
	}
}

/*
 * Waits for the previous set of snapshots to complete
 *
 * return         0 on success, -1 on failure
 */
int snap_join(void){
	return syscall(351);
}

#endif
