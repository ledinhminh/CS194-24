/* cs194-24 Lab 2 */

#include "cbs_proc.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>


int read_snap(char *buf,char **start,off_t offset,int count,int *eof,void *data ) 
{
	int len=0;

	len  += sprintf(buf+len, "Hello world");

	   
	return len;
}

void create_new_snap_entry() 
{
	create_proc_read_entry("test",0,NULL,read_snap,NULL);

}


int snap_init (void) {
	int ret;

	create_new_snap_entry();
	return 0;
}

void snap_cleanup(void) {
	remove_proc_entry("test",NULL);
}

module_init(snap_init);
module_exit(snap_cleanup);