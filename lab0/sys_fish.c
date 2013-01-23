#include "fish_impl.h"
#include "fish_compat.h"

#include <asm/io.h>

extern char *vga_mem;

long sys_fish(long fn, long a, long b, long c, long d)
{
    /* Map VGA memory into the kernel at a predictable address. */
    if (vga_mem == NULL)
	vga_mem = ioremap(0xB8000, 4000);

    return -1;
}

void fish_tasklet(unsigned long arg __attribute__((unused)))
{
}
