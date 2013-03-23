#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>


int read_proc(char *buf,char **start,off_t offset,int count,int *eof,void *data ) 
{
int len=0;

len  += sprintf(buf+len, "Hello world");

   
return len;
}

void create_new_proc_entry() 
{
create_proc_read_entry("hello",0,NULL,read_proc,NULL);

}


int proc_init (void) {
 int ret;
 
 create_new_proc_entry();
 return 0;
}

void proc_cleanup(void) {
 remove_proc_entry("hello",NULL);
}

MODULE_LICENSE("GPL"); 
module_init(proc_init);
module_exit(proc_cleanup);
