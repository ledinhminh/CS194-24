#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <../../../fs/proc/cbs_proc.h> //need for cbs_proc_t, seems hacky...
#define CBS_MAX_HISTORY 64
#define SNAP_MAX_TRIGGERS 8

//make sure synced with cbs_proc.c
struct snap_bucket {
	int valid;
	enum snap_event s_event;
	enum snap_trig s_trig;
	int device;
	int bucket_depth;
	struct cbs_proc * history;
};

//make sure synced with cbs_proc.c
struct snap_buffer {
	struct snap_bucket * buckets;
	int num_buckets;
};

//make sure synced with cbs_proc.c
struct cbs_proc {
	long pid;
	cbs_time_t creation_time;
	cbs_time_t start_time;
	cbs_time_t end_time; //initialize to -1
	cbs_time_t period;
	cbs_time_t compute_time;
	enum cbs_state state;
	int valid; //1 if it is, 0 if its not
	int is_next; //if this proc is the next proc
	struct cbs_proc* next; //pointer to next struct in the list
}; 

//mutex for EVERYTHING!!!
DEFINE_MUTEX(lock);
int running;

//list of buckets that hold trigger histories
struct snap_bucket bucket_list[SNAP_MAX_TRIGGERS];

//buffer that holds all the buckets
struct snap_buffer buffer = {
	.buckets = bucket_list,
	.num_buckets = 0,
};

//adds a cbs_proc_t to the bucket
int add_cbs_proc(int bucket_num, struct cbs_proc p){

	mutex_lock(&lock);
	//make sure bucket exists
	if(bucket_num >= buffer.num_buckets)
		return -1;

	//make sure bucket isn't full
	struct snap_bucket bucket = buffer.buckets[bucket_num];
	if(bucket.bucket_depth >= CBS_MAX_HISTORY)
		return -1;

	//add the proc to the end of the list
	int i;
	struct cbs_proc* proc_entry = bucket.history;
	for(i = 0; i < bucket.bucket_depth - 1; i++){
		proc_entry = proc_entry->next;		
	}
	if(proc_entry->next != NULL){
		//already exists and invalid history entry, just overwrite it
		proc_entry = proc_entry->next;
		proc_entry->pid = p.pid;
		proc_entry->creation_time = p.creation_time;
		proc_entry->start_time = p.start_time;
		proc_entry->end_time = p.end_time;
		proc_entry->period = p.period;
		proc_entry->compute_time = p.compute_time;
		proc_entry->state = p.state;
		proc_entry->valid = 1;
		//don't touch next pointer
	} else {
		//no history entry there, gotta do a kmalloc...
		struct cbs_proc *copy = kmalloc(sizeof(struct cbs_proc), GFP_KERNEL);
		copy->pid = p.pid;
		copy->creation_time = p.creation_time;
		copy->start_time = p.start_time;
		copy->end_time = p.end_time;
		copy->period = p.period;
		copy->compute_time = p.compute_time;
		copy->state = p.state;
		copy->valid = 1;
		copy->next = NULL;
	}

	//increment the bucket depth
	bucket.bucket_depth++;

	//gotta remember to free that lock
	mutex_unlock(&lock);
	return 0;
}

void invalidate_buffer(void){
	int i;
	for(i = 0; i < SNAP_MAX_TRIGGERS; i++){
		//go through each bucket and invalidate the history
		struct snap_bucket bucket = bucket_list[i];
		bucket.valid = 0;
		struct cbs_proc *proc_entry = bucket.history;
		
		if (proc_entry == NULL)
			return;

		while(proc_entry->next != NULL){
			proc_entry->valid = 0;
			proc_entry = proc_entry->next;
		}

		//set the bucket's depth (history length) to zero
		bucket.bucket_depth = 0;
	}
}

void check_done(void){
	//check if all the snapshots are all done
	int i;
	for (i = 0; i < buffer.num_buckets; i++){
		struct snap_bucket bucket = bucket_list[i];
		if(bucket.bucket_depth != CBS_MAX_HISTORY){
			break;
		}
	}
	mutex_lock(&lock);
	running = 0;
	mutex_unlock(&lock);
}

//wrapper for adding a proc, to make thing easier in the scheduler
void snap_add_proc(int bucket_num, long id, long creation, long start,
			 long end, long pd, long compute, enum cbs_state ste){
	struct cbs_proc to_add = {
		.pid = id,
		.creation_time = creation,
		.start_time = start,
		.end_time = end, //initialize to -1
		.period = pd,
		.compute_time = compute,
		.state = ste,
	};

	add_cbs_proc(bucket_num, to_add);
}

asmlinkage long sys_snapshot_join(void){
	while (running);
	return 0;
}

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
			running = 1;//TODO: FIGURE OUT HOW THIS WILL WORK
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

	//invalidate the buffer
	invalidate_buffer();

	//put in size into buffer
	buffer.num_buckets = n;

	//SET STUFF IN BUCKETS
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
	}

	//don't forget to unlock
	mutex_unlock(&lock);
	return 0;
}