/* CS194-24 Lab 0 "Fish" Test Harness */

#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "fish_impl.h"
#include "fish_syscall.h"

/* Maps VGA memory into this process's memory space -- this is only
 * necessary if we're running the tests from userspace. */
static void map_vga_memory(void);

/* Shuts down the system cleanly on an unrecoverable failure -- if we
 * don't do this the watchdog will kick in, but I don't like waiting
 * that long. */
static void shut_down(void);

/* Adds each character from a pair of files set to blink at a given
 * period. */
static void add_files(const char *on_filename, const char *off_filename,
		      unsigned short on_peroid, unsigned short off_period);

/* This runs a short demo program. */
static void demo(void);

/* Returns TRUE if the haystack starts with the needle */
static inline bool strsta(const char *haystack, const char *needle);

int main(int argc, char **argv)
{
    /* This sleep isn't strictly necessary, but it make the output a
     * bit easier to read -- without this sleep then some late kernel
     * messages will end up after the init, which makes it hard for
     * the user to tell when the kernel has booted. */
    sleep(1);

    /* This message tells the test framework that we've managed to
     * start up a process. */ 
    printf("[cs194-24] init running\n");

    map_vga_memory();

    /* Enable the system call tick source. */
    fish_syscall(__NR_fish, FISH_START, FISH_SOURCE_CALL);

    /* This is effectively a tiny little shell -- it allows us to call
     * the different system calls one at a time so the code can be
     * tested. */
    while (true)
    {
	char line[10240];

	/* Read a line from stdin, which will be attached to either
	 * the user or to the test haress.  Either way, we need a way
	 * to drive this test harness. */
	memset(line, '\0', 10240);
	fgets(line, 10240, stdin);
	while (strlen(line) > 0 && isspace(line[strlen(line) - 1]))
	    line[strlen(line) - 1] = '\0';

	/* We can never read NULL -- that would mean stdin is
	 * disconnected and we won't be getting any more commands.
	 * Note that this will probably just make the kernel panic. */
	if (line == NULL)
	    return 1;
	
	/* Here's the big switch of commands this test harness knows
	 * about.  Ech of these will probably map to a fish system
	 * call. */
	if (strcmp(line, "poweroff -f") == 0)
	    shut_down();
	else if (strsta(line, "add_files "))
	{
	    char *onstr, *offstr, *onper, *offper;

	    onstr = strstr(line+1, " ");
	    offstr = strstr(onstr+1, " ");
	    onper = strstr(offstr+1, " ");
	    offper = strstr(onper+1, " ");

	    *(onstr++) = '\0';
	    *(offstr++) = '\0';
	    *(onper++) = '\0';
	    *(offper++) = '\0';

	    add_files(onstr, offstr, atoi(onper), atoi(offper));
	}
	else if (strcmp(line, "clear") == 0)
	    fish_syscall(__NR_fish, FISH_CLEAR);
	else if (strcmp(line, "tick") == 0)
	    fish_syscall(__NR_fish, FISH_TICK, FISH_SOURCE_CALL);
	else if (strsta(line, "add_char "))
	{
	    char *x, *y, *onchr, *offchr, *onper, *offper;
	    struct fish_blink blink;
	    
	    x = strstr(line+1, " ");
	    y = strstr(x+1, " ");
	    onchr = strstr(y+1, " ");
	    offchr = strstr(onchr+1, " ");
	    onper = strstr(offchr+1, " ");
	    offper = strstr(onper+1, " ");

	    *(x++) = '\0';
	    *(y++) = '\0';
	    *(onchr++) = '\0';
	    *(offchr++) = '\0';
	    *(onper++) = '\0';
	    *(offper++) = '\0';

	    blink.x_posn = atoi(x);
	    blink.y_posn = atoi(y);
	    blink.on_char = onchr[0];
	    blink.off_char = offchr[0];
	    blink.on_period = atoi(onper);
	    blink.off_period = atoi(offper);

	    fish_syscall(__NR_fish, FISH_ADD, &blink);
	}
	else if (strsta(line, "remove "))
	{
	    char *x, *y;
	    
	    x = strstr(line+1, " ");
	    y = strstr(x+1, " ");

	    *(x++) = '\0';
	    *(y++) = '\0';

	    fish_syscall(__NR_fish, FISH_REMOVE, atoi(x), atoi(y));
	}
	else if (strsta(line, "sync "))
	{
	    char *fx, *fy, *tx, *ty;
	    
	    fx = strstr(line+1, " ");
	    fy = strstr(fx+1, " ");
	    tx = strstr(fy+1, " ");
	    ty = strstr(tx+1, " ");

	    *(fx++) = '\0';
	    *(fy++) = '\0';
	    *(tx++) = '\0';
	    *(ty++) = '\0';

	    fish_syscall(__NR_fish, FISH_SYNC, atoi(fx), atoi(fy),
			                       atoi(tx), atoi(ty));
	}
	else if (strcmp(line, "demo") == 0)
	    demo();
	else
	    printf("[fish] Unknown command: '%s'\n", line);

	printf("[fish] command '%s' finished\n", line);
    }

    return 0;
}

#ifdef USERSPACE_TEST
extern char *vga_mem;
void map_vga_memory(void)
{
    int mem_fd;

    mem_fd = open("/dev/mem", O_RDWR);
    if (mem_fd < 0)
    {
	perror("open /dev/mem");
	shut_down();
    }
    
    vga_mem = mmap(0, 1048576, PROT_READ | PROT_WRITE,
		   MAP_SHARED, mem_fd, 0);
    
    if (vga_mem == MAP_FAILED)
    {
	perror("mmap vga memory");
	shut_down();
    }

    vga_mem += 0xB8000;
}
#else
void map_vga_memory(void)
{
    /* There's nothing to do when we're running in kernel space */
    return;
}
#endif

void shut_down(void)
{
    /* This magic constant means to turn off the system after halting
     * -- otherwise it'll just sit there spinning with a QEMU window
     * still up. */
    sync();
    reboot(0x4321fedc);
}

void add_files(const char *on_filename, const char *off_filename,
	       unsigned short on_period, unsigned short off_period)
{
    FILE *on_file, *off_file;
    struct fish_blink blink;

    on_file = fopen(on_filename, "r");
    if (on_file == NULL)
    {
	perror("Unable to open on_file");
	shut_down();
    }

    off_file = fopen(off_filename, "r");
    if (off_file == NULL)
    {
	perror("Unable to open off_file");
	shut_down();
    }

    /* Initialize the blink structure to start writing the fish to the
     * start of the screen. */
    blink.x_posn = 0;
    blink.y_posn = 0;
    blink.on_period = on_period;
    blink.off_period = off_period;

    /* Read the entire input file (both should be the same size but
     * there's not particularly good checking for that. */
    while ((blink.on_char = fgetc(on_file)) != EOF)
    {
	/* If we've been given a short second file then just give up
	 * and die here */
	blink.off_char = fgetc(off_file);

	/* The newline character shouldn't be printed, it should
	 * instead end cause a newline to happen. */
	if (blink.on_char == '\n')
	{
	    blink.x_posn = 0;
	    blink.y_posn++;
	    continue;
	}

	/* Every normal character just gets added to the blink list. */
	fish_syscall(__NR_fish, FISH_ADD, &blink);
	blink.x_posn++;
    }
}

bool strsta(const char *haystack, const char *needle)
{
    return strncmp(haystack, needle, strlen(needle)) == 0;
}

void demo(void)
{
    fish_syscall(__NR_fish, FISH_START, FISH_SOURCE_CALL);

    fish_syscall(__NR_fish, FISH_CLEAR);

    add_files("/frame0.txt", "/frame1.txt", 10, 10);

#ifdef USERSPACE_TEST
    {
	int i;

	for (i = 0; i < 100; i++)
	{
	    fish_syscall(__NR_fish, FISH_TICK, FISH_SOURCE_CALL);
	    usleep(100 * 1000);
	}
    }
#else
    fish_syscall(__NR_fish, FISH_START, FISH_SOURCE_RTC);
    sleep(10);
#endif

    {
	struct fish_blink blink;

	blink.x_posn = 20;
	blink.y_posn = 5;
	blink.on_char = 'M';
	blink.off_char = 'I';
	blink.on_period = 3;
	blink.off_period = 5;

	fish_syscall(__NR_fish, FISH_REMOVE, blink.x_posn, blink.y_posn);
	fish_syscall(__NR_fish, FISH_ADD, &blink);
    }

#ifdef USERSPACE_TEST
    {
	int i;

	for (i = 0; i < 100; i++)
	{
	    fish_syscall(__NR_fish, FISH_TICK, FISH_SOURCE_CALL);
	    usleep(100 * 1000);
	}
    }
#else
    fish_syscall(__NR_fish, FISH_START, FISH_SOURCE_RTC);
    sleep(10);
#endif

    fish_syscall(__NR_fish, FISH_SYNC, 0, 0, 20, 5);

#ifdef USERSPACE_TEST
    {
	int i;

	for (i = 0; i < 100; i++)
	{
	    fish_syscall(__NR_fish, FISH_TICK, FISH_SOURCE_CALL);
	    usleep(100 * 1000);
	}
    }
#else
    fish_syscall(__NR_fish, FISH_START, FISH_SOURCE_RTC);
    sleep(10);
    fish_syscall(__NR_fish, FISH_STOP, FISH_SOURCE_RTC);
#endif
}
