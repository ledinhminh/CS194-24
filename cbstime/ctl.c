#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef EXECPATH
#define EXECPATH ".obj/realtime"
#endif

static void try_bool(const char *haystack, const char *needle, bool *b)
{
    if (strncmp(haystack, needle, strlen(needle)) == 0)
	if (strcmp(haystack + strlen(needle), "true") == 0)
	    *b = true;
}

static void try_int(const char *haystack, const char *needle, int *i)
{
    if (strncmp(haystack, needle, strlen(needle)) == 0)
	*i = atoi(haystack + strlen(needle));
}

static void try_str(const char *haystack, const char *needle, char **c)
{
    if (strncmp(haystack, needle, strlen(needle)) == 0)
	*c = strdup(haystack + strlen(needle));
}

int main(int argc, char **argv)
{
    char *query_string;
    char *tok;

    bool start;
    char *mode;
    int period;
    int cpu;

    query_string = strdup(getenv("QUERY_STRING"));
    printf("QUERY_STRING: '%s'\n", query_string);

    start = false;
    mode = "CBS_RT";
    period = 100 * 1000;
    cpu = 1000;

    tok = strtok(query_string, "&");
    do
    {
	try_bool(tok, "start=" , &start );
	try_str (tok, "mode="  , &mode  );
	try_int (tok, "period=", &period);
	try_int (tok, "cpu="   , &cpu   );
    } while ((tok = strtok(NULL, "&")) != NULL);

    if (start)
    {
	if (fork() == 0)
	{
	    char *cpu_s;
	    char *sec_s;
	    char *usec_s;

	    asprintf(&cpu_s, "%d", cpu);
	    asprintf(&sec_s, "%d", period / (1000 * 1000));
	    asprintf(&usec_s, "%d", period % (1000 * 1000));
	    execl(EXECPATH, EXECPATH, cpu_s, sec_s, usec_s, NULL);
	    abort();
	}

	return 0;
    }

    fprintf(stderr, "No method found\n");
    abort();
}
