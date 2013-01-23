/* CS194-24 Lab 1 Fish Syscall Implementation */

#include "fish_impl.h"
#include "fish_compat.h"

/* Linux virtualizes the VGA memory in both userspace and kernelspace:
 * in userspace we need to mmap it from a file, and in kernelspace we
 * need to map it from the kernel's virtual address space. */
char *vga_mem = NULL;

long fish_clear(void)
{
    return -1;
}

long fish_add(struct fish_blink *to_add)
{
    return -1;
}

long fish_remove(short x, short y)
{
    return -1;
}

long fish_find(struct fish_blink *to_find)
{
    return -1;
}

long fish_sync(short fx, short fy, short tx, short ty)
{
    return -1;
}

long fish_start(int i)
{
    return -1;
}

long fish_stop(int i)
{
    return -1;
}

long fish_tick(int index)
{
    return -1;
}
