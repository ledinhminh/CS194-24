/* CS194-24 Lab 1 "Fish" Syscall Implementation */

#ifndef FISH_SYSCALLS_H
#define FISH_SYSCALLS_H

#define __NR_fish   -1
#define FISH_CLEAR  0x00
#define FISH_ADD    0x01
#define FISH_REMOVE 0x02
#define FISH_FIND   0x03
#define FISH_SYNC   0x04
#define FISH_START  0x05
#define FISH_STOP   0x06
#define FISH_TICK   0x07

#define FISH_SOURCE_RTC  0x00
#define FISH_SOURCE_CALL 0x01

struct fish_blink
{
    /* The position of this character on the screen */
    unsigned short x_posn;
    unsigned short y_posn;

    /* The given location will iterate between these two characters. */
    char on_char;
    char off_char;

    /* The period of the iteration. */
    unsigned short on_period;
    unsigned short off_period;
};

/* These system call implementations are used directly by the
 * userspace test harness.  You should not call these directly from
 * anywhere in the test harness, all calls to them should pass through
 * lab0_syscall() -- otherwise you won't be able to link when you try
 * to put your code in the kernel. */

long fish_clear(void);

long fish_add(struct fish_blink *to_add);

long fish_remove(short x, short y);

long fish_find(struct fish_blink *to_find);

long fish_sync(short fx, short fy, short tx, short ty);

long fish_start(int i);

long fish_stop(int i);

long fish_tick(int i);

#endif
