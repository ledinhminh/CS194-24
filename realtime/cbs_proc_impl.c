/* cs194-24 Lab 2 */

#include "cbs_proc.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>

//not sure if we need to keep this around unless we want to do cleanup...
static struct proc_dir_entry *proc_parent;

int read_snap(char *buf,char **start,off_t offset,int count,int *eof,void *data ) 
{
	int len = 0;
	struct snap_proc_data *d = data;
	len = cbs_snap(buf, d->num);
	return len;
}

void create_new_snap_entry(void) 
{
	proc_parent = proc_mkdir("snapshot", NULL);
	if (!proc_parent){
		return -ENOMEM;
	}
	int i;
	for(i = 0; i < SNAP_MAX_TRIGGERS; i++){
		char buffer[16];
		sprintf(buffer, "%d", i);
		struct snap_proc_data *data = kmalloc(sizeof(struct snap_proc_data), GFP_KERNEL);
		data->num = i;
		create_proc_read_entry(buffer, 0, proc_parent, read_snap, data);
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