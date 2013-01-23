/* CS194-24 Lab 1 Fish Compatibility Layer */

#ifndef FISH_COMPAT_H
#define FISH_COMPAT_H

/* The idea behind this header is that most code isn't portable
 * between kernel space and user space.  This provides an interface by
 * which one codebase will run in both userspace and kernel space. */
#ifdef USERSPACE_TEST

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* These don't matter in userspace, as we can only access USER memory
 * and we can always sleep.  They're important in kernel space,
 * though! */
#define GFP_KERNEL 0
#define GFP_USER 0
#define GFP_ATOMIC 0

static inline void *kmalloc(size_t size, ...)
{
    return malloc(size);
}

static inline void kfree(void *ptr)
{
    return free(ptr);
}

static inline void copy_from_user(void *to, const void *from, unsigned long n)
{
    memcpy(to, from, n);
}

static inline void copy_to_user(void *to, const void *from, unsigned long n)
{
    memcpy(to, from, n);
}

#else

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/gfp.h>

#endif

#endif

