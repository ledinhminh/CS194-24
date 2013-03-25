/* cs194-24 Lab 2 
 * THE PROC INTERFACE AND NOTHING BUT THAT
*/

#include "cbs_proc.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/syscalls.h>


//not sure if we need to keep this around unless we want to do cleanup...
static struct proc_dir_entry *proc_parent;

void get_all(cbs_proc_t p, void *buf){
	int len;
	len = strlen(buf);

	len += sprintf(buf+len, "PID: %li\n", cbs_get_pid(p));
	len += sprintf(buf+len, "CREATION TIME: %li\n", cbs_get_ctime(p));
	len += sprintf(buf+len, "START TIME: %li\n", cbs_get_start(p));
	len += sprintf(buf+len, "END TIME: %li\n", cbs_get_end(p));
	len += sprintf(buf+len, "PERIOD: %li\n", cbs_get_period(p));
	len += sprintf(buf+len, "COMPUTE: %li\n", cbs_get_compute(p));
	switch(cbs_get_state(p)){
		case CBS_STATE_HISTORY:
			len += sprintf(buf+len, "STATE: HISTORY\n\n");
			break;
		case CBS_STATE_RUNNING:
			len += sprintf(buf+len, "STATE: RUNNING\n\n");
			break;
		case CBS_STATE_READY:
			len += sprintf(buf+len, "STATE: READY\n\n");
			break;
		case CBS_STATE_BLOCKED:
			len += sprintf(buf+len, "STATE: BLOCKED\n\n");
			break;
		case CBS_STATE_INVALID:
			len += sprintf(buf+len, "STATE: INVALID\n\n");
			break;
		default:
			len += sprintf(buf+len, "STATE: UNKNOWN\n\n");
			break;
	}

}

int read_snap(char *buf,char **start,off_t offset,int count,int *eof,void *data ) 
{
	int len = 0;
	struct snap_proc_data *d = data;
	len = cbs_snap(buf, d->num);
	cbs_list_history(d->num, &get_all, buf);
	len = strlen(buf);
	return len;
}

int write_snap(struct file *file,const char *buf,int count,void *data )
{
	char proc_data[count];
	char fill[] = "fill\n"; //echo adds a newline
	char stop[] = "stop\n";
	char start[] = "start\n";
	if(copy_from_user(proc_data, buf, count))
	    return -EFAULT;

	if(strcmp(proc_data, fill) == 0){
		printk("FILLING SNAPS WITH JUNK...\n");
		fill_snap(); //test thing to fill up snap buckets
	} else if(strcmp(proc_data, stop) == 0){
		printk("STOPPING CBS...\n");
		cbs_disable();
	} else if(strcmp(proc_data, start) == 0){
		printk("STARTING CBS...\n");
		cbs_enable();
	} else {
		printk("You only got three options: fill, start, or stop");
	}

	return count;
}

void create_new_snap_entry(void) 
{
	int i;
	proc_parent = proc_mkdir("snapshot", NULL);
	if (proc_parent){
		for(i = 0; i < SNAP_MAX_TRIGGERS; i++){
			char buffer[16];
			sprintf(buffer, "%d", i);
			struct snap_proc_data *data = kmalloc(sizeof(struct snap_proc_data), GFP_KERNEL);
			data->num = i;
			struct proc_dir_entry *e;
			e = create_proc_entry(buffer, 0, proc_parent);
			e->read_proc = read_snap;
			e->write_proc = write_snap;
			e->data = (void *) data;
		}
	}
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