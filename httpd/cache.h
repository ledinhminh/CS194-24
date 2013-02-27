/* cs194-24 Lab 1 */

// Just implement a shitty linked list because I have no clue what I'm doing.
#ifndef CACHE_H
#define CACHE_H

#include "palloc.h"
#include "http.h"

struct cache_entry
{
    const char* request;
    char* response;
    int used;
    struct cache_entry* next;
};

void cache_init(palloc_env env);
void cache_add(const char* request, char* response);
void cache_print(void);
char* cache_lookup(palloc_env env, const char* request);

#endif
