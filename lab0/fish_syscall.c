/* CS194-24 Lab 0 "Fish" System Call Interface */

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "fish_syscall.h"
#include "fish_impl.h"

#ifdef USERSPACE_TEST
long fish_syscall(int nr, int fn, ...)
{
    long out;
    va_list args;

    if (__NR_fish != nr)
	return -1;

    out = -1;
    va_start(args, fn);

    switch (fn)
    {
    case FISH_CLEAR:
	out = fish_clear();
	break;
    case FISH_ADD:
	out = fish_add(va_arg(args, struct fish_blink *));
	break;
    case FISH_REMOVE:
    {
	int iargs[2];

	iargs[0] = va_arg(args, int);
	iargs[1] = va_arg(args, int);
	out = fish_remove(iargs[0], iargs[1]);
	break;
    }
    case FISH_FIND:
	out = fish_find(va_arg(args, struct fish_blink *));
	break;
    case FISH_SYNC:
    {
	int iargs[4];

	iargs[0] = va_arg(args, int);
	iargs[1] = va_arg(args, int);
	iargs[2] = va_arg(args, int);
	iargs[3] = va_arg(args, int);
	out = fish_sync(iargs[0], iargs[1], iargs[2], iargs[3]);
	break;
    }
    case FISH_START:
	out = fish_start(va_arg(args, int));
	break;
    case FISH_STOP:
	out = fish_stop(va_arg(args, int));
	break;
    case FISH_TICK:
	out = fish_tick(va_arg(args, int));
	break;
    default:
	printf("Unknown Fish command\n");
	break;
    }

    va_end(args);
    return out;
}
#else
long fish_syscall(int nr, int fn, ...)
{
    return -1;
}
#endif
