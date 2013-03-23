#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>

#define CBS_MAX_HISTORY 64
#define SNAP_MAX_TRIGGERS 8

enum cbs_state
{
    CBS_STATE_HISTORY,   /* The result of a historical run */
    CBS_STATE_RUNNING,   /* The currently running process */
    CBS_STATE_READY,     /* Ready to run, in a queue somewhere */
    CBS_STATE_BLOCKED,   /* Unable to run for any reason */
    CBS_STATE_INVALID,   /* The request was not for a valid process */
};

struct snap_bucket {
	enum snap_event s_event;
	enum snap_trig s_trig;
	int device;
	int bucket_depth;
	//and some pointer to a list of cbs_proc_t
};

struct snap_buffer {
	struct snap_bucket * buckets;
	int num_buckets;
};


DEFINE_MUTEX(lock);
int running;

//list of buckets that hold trigger histories
struct snap_bucket bucket_list[SNAP_MAX_TRIGGERS];

//buffer that holds all the buckets
struct snap_buffer buffer = {
	.buckets = bucket_list,
	.num_buckets = 0,
};

asmlinkage long sys_snapshot(enum snap_event __user *events, int __user *device,
	     enum snap_trig __user *triggers, size_t n)
{
	//CHECK TO SEE IF ANOTHER SNAPSHOT IS STILL RUNNING
	//IF IT IS, RETURN EAGAIN
	//  running is set to off when all snapshots are finished being taken
	if(running){
		return EAGAIN;
	} else {
		if(mutex_trylock(&lock)){
			running = 1;
		} else {
			//someone else has the lock
			//just say EAGAIN to be safe
			return EAGAIN;
		}
	}

	int index;
	enum snap_event kern_events[n];
	enum snap_trig kern_triggers[n];
	int kern_device[n];

	//COPY STUFF FROM USERSPACE TO KERNELSPACE
	//copy events
	if(copy_from_user(&kern_events, events, sizeof(enum snap_event)*n)){
		return -EFAULT;
	}
	//copy triggers
	if(copy_from_user(&kern_triggers, triggers, sizeof(enum snap_trig)*n)){
		return -EFAULT;
	}
	//copy devices
	if(copy_from_user(&kern_device, device, sizeof(int)*n)){
		return -EFAULT;
	}

	//put in size into buffer
	buffer.num_buckets = n;
	//SET STUFF IN BUFFERS
	for (index = 0; index < n; index++){
		//set the event of a bucket
		buffer.buckets[index].s_event = kern_events[index];
		if(kern_events[index] == SNAP_EVENT_CBS_SCHED){
			printk("Snap for CBS scheduler\n");
		} else {
			printk("Snap for unknown scheduler\n");
		}

		//set the trigger of a bucket
		buffer.buckets[index].s_trig= kern_triggers[index];
		if(kern_triggers[index] == SNAP_TRIG_BEDGE){
			printk("Snap for BEDGE\n");
		} else if (kern_triggers[index] == SNAP_TRIG_AEDGE){
			printk("Snap for AEDGE\n");
		} else {
			printk("Snap for unknown time\n");
		}

		//set the device of a bucket
		buffer.buckets[index].device = kern_device[index];
		printk("Snap for device %i\n",kern_device[index]);

		//reset depth of a bucket
		buffer.buckets[index].bucket_depth = 0;
	}


	//don't forget to unlock
	mutex_unlock(&lock);
	return 0;
}