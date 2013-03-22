#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>

DEFINE_MUTEX(lock);
int running;

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
			running = 1;
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
	if(copy_from_user(&kern_events, events, sizeof(enum snap_event)*n)){
		return -EFAULT;
	}

	if(copy_from_user(&kern_triggers, triggers, sizeof(enum snap_trig)*n)){
		return -EFAULT;
	}

	if(copy_from_user(&kern_device, device, sizeof(int)*n)){
		return -EFAULT;
	}

	//PRINT STUFF OUT FOR DEBUGGING
	for (index = 0; index < n; index++){
		if(kern_events[index] == SNAP_EVENT_CBS_SCHED){
			printk("Snap for CBS scheduler\n");
		} else {
			printk("Snap for unknown scheduler\n");
		}

		if(kern_triggers[index] == SNAP_TRIG_BEDGE){
			printk("Snap for BEDGE\n");
		} else if (kern_triggers[index] == SNAP_TRIG_AEDGE){
			printk("Snap for AEDGE\n");
		} else {
			printk("Snap for unknown time\n");
		}

		printk("Snap for device %i\n",kern_device[index]);
	}
	//don't forget to unlock
	mutex_unlock(&lock);
	return 0;
}