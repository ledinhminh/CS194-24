/* cs194-24 Lab 2 */

#include "cbs_proc.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>

#define MAX_PROC_SIZE 100

static char proc_data[MAX_PROC_SIZE];

static struct proc_dir_entry *proc_write_entry;
static struct proc_dir_entry *proc_parent;

int read_proc(char *buf,char **start,off_t offset,int count,int *eof,void *data )
{
    int len=0;
    len = sprintf(buf,"\n %s\n ",proc_data);

    return len;
}

int write_proc(struct file *file,const char *buf,int count,void *data )
{

    if(count > MAX_PROC_SIZE)
        count = MAX_PROC_SIZE;
    if(copy_from_user(proc_data, buf, count))
        return -EFAULT;

    return count;
}

void create_new_snapshot_proc_entry()
{
    proc_parent = proc_mkdir("snapshot",NULL);
    if(!proc_parent)
    {
        printk(KERN_INFO "Error creating proc entry");
        return -ENOMEM;
    }

    proc_write_entry = create_proc_entry("0",0666,proc_parent);
    if(!proc_write_entry)
    {
        printk(KERN_INFO "Error creating proc entry");
        return -ENOMEM;
    }

    proc_write_entry->read_proc = read_proc;
    proc_write_entry->write_proc = write_proc;

}



int proc_init (void) {
    create_new_snapshot_proc_entry();
    return 0;
}

module_init(proc_init);
