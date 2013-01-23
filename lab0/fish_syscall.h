/* CS194-24 Lab 0 "Fish" System Call Interface */

#ifndef FISH_SYSTEMCALL_H
#define FISH_SYSTEMCALL_H

#include <stdint.h>

/* This system call interface allows us to arbitrate between function
 * calls (for the userspace test) and system calls (for the kernel
 * test). */
intptr_t fish_syscall(int nr, int fn, ...);

#endif
