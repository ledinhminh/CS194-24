/* cs194-24 Lab 2 */

#include "cbs_proc.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/syscalls.h>


//not sure if we need to keep this around unless we want to do cleanup...
static struct proc_dir_entry *proc_parent;
#define MAX_PROC_SIZE 100

void fill(){
	printk("watup!");
}

int read_snap(char *buf,char **start,off_t offset,int count,int *eof,void *data ) 
{
	int len = 0;
	struct snap_proc_data *d = data;
	len = cbs_snap(buf, d->num);
	return len;
}

int write_snap(struct file *file,const char *buf,int count,void *data )
{
	char proc_data[count];
	char cmd[] = "fill\n";
	if(count > MAX_PROC_SIZE)
	    count = MAX_PROC_SIZE;
	if(copy_from_user(proc_data, buf, count))
	    return -EFAULT;

	printk("%s\n", proc_data);
	int i = strcmp(proc_data, cmd);
	printk("%i\n", i);
	int j = memcmp(proc_data, cmd, count);
	printk("%i\n", j);

	return count;
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
		struct proc_dir_entry *e;
		e = create_proc_entry(buffer, 0, proc_parent);
		e->read_proc = read_snap;
		e->write_proc = write_snap;
		e->data = (void *) data;
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