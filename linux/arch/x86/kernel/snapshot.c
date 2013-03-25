#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/completion.h>

#include <../../../fs/proc/cbs_proc.h> //need for cbs_proc_t, seems hacky...
#define CBS_MAX_HISTORY 64
#define SNAP_MAX_TRIGGERS 8

//make sure synced with cbs_proc.c
struct snap_bucket {
	int valid; //need this to know whether we should do bedge or aedge stuff
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
DECLARE_COMPLETION(run_lock);


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

	struct cbs_proc* proc_entry;
	struct cbs_proc* prev_proc_entry;
	struct snap_bucket* bucket;
	int i;
	mutex_lock(&lock);
	
	//make sure bucket exists
	if(bucket_num >= buffer.num_buckets)
		return -1;

	//make sure bucket isn't full
	bucket = &(buffer.buckets[bucket_num]);
	if(bucket->bucket_depth >= CBS_MAX_HISTORY)
		return -1;

	//add the proc to the end of the list
	proc_entry = bucket->history;
	prev_proc_entry = NULL;
	for(i = 0; i < bucket->bucket_depth; i++){
		prev_proc_entry = proc_entry;
		proc_entry = proc_entry->next;
	}

	if(proc_entry != NULL){
		proc_entry = proc_entry->next;
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
		//set the previous entry's next to this copy
		if(prev_proc_entry != NULL){
			prev_proc_entry->next = copy;
		} else {
			//first thing in the list
			bucket->history = copy;
		}
	}

	//increment the bucket depth
	bucket->bucket_depth++;

	//gotta remember to free that lock
	mutex_unlock(&lock);
	return 0;
}

//marks every bucket and entry invalid
void invalidate_buffer(void){
	int i;
	struct snap_bucket bucket;
	struct cbs_proc *proc_entry;

	for(i = 0; i < SNAP_MAX_TRIGGERS; i++){
		//go through each bucket and invalidate the history
		bucket = bucket_list[i];
		bucket.valid = 0;
		proc_entry = bucket.history;
		
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

void snap_mark_state(int cpu_id, long proc_id, enum cbs_state state){
	int i;
	int j;
	struct snap_bucket *bucket;
	struct cbs_proc *entry;
	for (i = 0; i < buffer.num_buckets; i++){
		bucket = &(bucket_list[i]);
		if (bucket->device == cpu_id){
			for (j = 0; j < bucket->bucket_depth; i++){
				entry = bucket->history;
				if (entry->pid == proc_id){
					entry->state = state;
				}
			}
		}
	}
}

void snap_mark_history(int cpu_id, long proc_id){
	snap_mark_state(cpu_id, proc_id, CBS_STATE_HISTORY);
}

void snap_mark_running(int cpu_id, long proc_id){
	snap_mark_state(cpu_id, proc_id, CBS_STATE_RUNNING);
}

void snap_mark_blocked(int cpu_id, long proc_id){
	snap_mark_state(cpu_id, proc_id, CBS_STATE_BLOCKED);
}

void snap_mark_invalid(int cpu_id, long proc_id){
	snap_mark_state(cpu_id, proc_id, CBS_STATE_INVALID);
}


//wrapper for adding a proc, to make thing easier in the scheduler
void snap_add_ready(int cpu_id, long proc_id, long creation, long start,
			 long end, long pd, long compute)
{
	int i;
	struct cbs_proc to_add = {
		.pid = proc_id,
		.creation_time = creation,
		.start_time = start,
		.end_time = end, //initialize to -1
		.period = pd,
		.compute_time = compute,
		.state = CBS_STATE_READY,
	};

	//need add to every bucket that has the same device as cpu_id
	for(i = 0; i < buffer.num_buckets; i++){
		if (bucket_list[i].device == cpu_id){
			add_cbs_proc(i, to_add);
		}
	}

}

//fills valid buckets with some history
void fill_snap(void){
	int i;
	int j;
	//using bucket number for cpu_id...
	//this only works if you ran snap_test beforehand
	for (i = 0; i < buffer.num_buckets; i++){
		for (j = 0; j < 10; j++){
			snap_add_ready(i, j, 111, 222, 333, 444, 555);
		}
	}
}

asmlinkage long sys_snapshot_join(void){
	wait_for_completion(&run_lock);
	return 0;
}

asmlinkage long sys_snapshot(enum snap_event __user *events, int __user *device,
	     enum snap_trig __user *triggers, size_t n)
{
	//CHECK TO SEE IF ANOTHER SNAPSHOT IS STILL RUNNING
	//IF IT IS, RETURN EAGAIN
	//  running is set to off when a snapshot syscall is finished
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

		//make the bucket valid
		buffer.buckets[index].valid = 1;
	}

	//don't forget to unlock
	running = 0;
	complete(&run_lock);
	mutex_unlock(&lock);
	return 0;
}