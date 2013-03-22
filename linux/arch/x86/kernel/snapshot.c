#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>


asmlinkage long sys_snapshot(void)
{
	printk("OH SNAP! IT WORKS!\n");
	return 0;
}
