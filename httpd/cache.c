/* cs194-24 Lab 1 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "palloc.h"
#include "cache.h"
#include "debug.h"

#define CACHE_SIZE 512

// Shitty second-chance algorithm.
// This is not thread-safe. At all. In fact, it's not safe for anything.

palloc_env* envp;
int num_entries;
struct cache_entry* cache;
struct cache_entry* tail;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void cache_init(palloc_env env) {
    envp = &env;
    num_entries = 0;
    cache = palloc(env, struct cache_entry);
    memset(cache, 0, sizeof(struct cache_entry));
    tail = cache;
    
    DEBUG("CACHE_SIZE=%d, envp=%p, cache=%p\n", CACHE_SIZE, envp, cache);
}

/* String will be duplicated. Caller is responsible for deallocating the passed-in string */
void cache_add(const char* request, char* response) {
    int add_entry = 0;
    
    // We get called even if errno ENOENT up the anus.
    if (NULL == response) {
        DEBUG("NULL response. what were you thinking?\n");
        return;
    }
    
    DEBUG("num_entries=%d, cache=%p, tail=%p\n", num_entries, cache, tail);
    
    pthread_mutex_lock(&lock);
    
    if (CACHE_SIZE == num_entries) { // Time to shuffle.
        INFO; printf("cache is full\n");
        // Is the page used?
        if (1 == cache->used) { // Used, move to tail
            DEBUG("first entry (%s) used, moving\n", cache->request);
            struct cache_entry* temp;
            tail->next = cache;
            temp = cache->next; // The new cache
            cache->used = 0;
            cache->next = NULL;
            tail = cache;
            cache = temp;
            pthread_mutex_unlock(&lock);
        } else { // Not used, free entry and add new entry
            // This sounds inefficient but palloc hands us the chunk of memory right back
            DEBUG("entry (%s) not used, deleting\n", cache->request);
            struct cache_entry* temp = cache;
            cache = cache->next;
            pfree(temp);
            
            add_entry = 1;
        }
    } else { // Add entry -- possibly first entry
        DEBUG("cache not full\n");
        num_entries++;
        add_entry = 1;
    }
    
    if (1 == add_entry) {
        DEBUG("adding entry (%s)\n", request);
        // Cache is not empty, make new entry
        if (NULL != cache->request) {
            struct cache_entry* new_entry = palloc(&envp, struct cache_entry);
            tail->next = new_entry;
            tail = new_entry;
        }

        tail->request = palloc_strdup(tail, request);
        tail->response = palloc_strdup(tail, response);
        tail->next = NULL;
        pthread_mutex_unlock(&lock);
    }
}

void cache_print(void) {
    struct cache_entry* entry = cache;
    do {
        DEBUG("entry: request=%s response=%s\n", entry->request, entry->response);
    } while (NULL != (entry = entry->next));
}

/* String will be duplicated. Caller is responsible for deallocating the returned string */
// Yes, this is a linear search... 
// We'll just take the env that the caller has allocated into, to make life easier for all involved parties.
char* cache_lookup(palloc_env env, const char* request) {
    DEBUG("request=%s\n", request);
    struct cache_entry* entry = cache;
    // Go through this awkward dance to not skip the first one.
    if (NULL == entry->request) {
        return NULL;
    }
        
    do {
        if (0 == strcmp(request, entry->request)) {
            entry->used = 1;
            return palloc_strdup(env, entry->response);
        }
    } while (NULL != (entry = entry->next));
    return NULL;
}