/* cs194-24 Lab 2 */

#include "cbs_proc.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
enum snap_event
{
    SNAP_EVENT_CBS_SCHED,     /* Triggers when the CBS scheduler
			       * context switches a task */
};

enum snap_trig
{
    SNAP_TRIG_BEDGE,    /* Triggers on the edge before an event starts */
    SNAP_TRIG_AEDGE,    /* Triggers on the after before an event starts */
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


extern struct snap_buffer buffer;

int read_snap(char *buf,char **start,off_t offset,int count,int *eof,void *data ) 
{
	int len=0;

	int index;

	for(index = 0; index < buffer.num_buckets; index++){
		//get the bucket
		struct snap_bucket bucket = buffer.buckets[index];

		//print the bucket number
		len += sprintf(buf+len, "Bucket #%i\n", index);

		//print the event
		if (bucket.s_event == SNAP_EVENT_CBS_SCHED){
			len += sprintf(buf+len, "EVENT: CBS\n");
		} else {
			len += sprintf(buf+len, "EVEVNT: UNKNOWN\n");
		}

		//print the trigger
		if (bucket.s_trig == SNAP_TRIG_AEDGE){
			len += sprintf(buf+len, "TRIG: AEDGE\n");
		} else if (bucket.s_trig == SNAP_TRIG_BEDGE){
			len += sprintf(buf+len, "TRIG: BEDGE\n");
		} else {
			len += sprintf(buf+len, "TRIG: UKNOWN\n");
		}

		//add some lines to make the output pretty
		len += sprintf(buf+len, "\n\n");
	}

	return len;
}

void create_new_snap_entry() 
{
	create_proc_read_entry("test",0,NULL,read_snap,NULL);

}


int snap_init (void) {
	create_new_snap_entry();
	return 0;
}

void snap_cleanup(void) {
	remove_proc_entry("test",NULL);
}

module_init(snap_init);
module_exit(snap_cleanup);