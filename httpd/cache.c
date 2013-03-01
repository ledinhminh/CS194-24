/* cs194-24 Lab 1 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "palloc.h"
#include "cache.h"
#include "debug.h"

#define CACHE_SIZE 512

// Cache with second-chance replacement algorithm.
// Yes, I know it's backed by a linked list.
// Protected by a list-global lock. Enjoy that.

palloc_env* envp;
int num_entries;
struct cache_entry* cache;
struct cache_entry* tail;
pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;

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
        return;
    }
    
    DEBUG("num_entries=%d, cache=%p, tail=%p\n", num_entries, cache, tail);
    
    DEBUG("waiting for rwlock\n");
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
    pthread_rwlock_wrlock(&lock);
    DEBUG("got rwlock\n");
    
    if (CACHE_SIZE == num_entries) { // Time to shuffle.
        DEBUG("cache is full\n");
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
    }
    pthread_rwlock_unlock(&lock);
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
    DEBUG("released rwlock\n");
}

void cache_print(void) {
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
    pthread_rwlock_rdlock(&lock);
    struct cache_entry* entry = cache;
    do {
        DEBUG("entry: request=%s response=%s\n", entry->request, entry->response);
    } while (NULL != (entry = entry->next));
    pthread_rwlock_unlock(&lock);
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
}

/* String will be duplicated. Caller is responsible for deallocating the returned string */
// Yes, this is a linear search... 
// We'll just take the env that the caller has allocated into, to make life easier for all involved parties.
char* cache_lookup(palloc_env env, const char* request) {
    DEBUG("request=%s\n", request);
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
    pthread_rwlock_rdlock(&lock);
    struct cache_entry* entry = cache;
    // Go through this awkward dance to not skip the first one.
    if (NULL == entry->request) {
        pthread_rwlock_unlock(&lock);
        DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
        return NULL;
    }
        
    do {
        if (0 == strcmp(request, entry->request)) {
            entry->used = 1;
            DEBUG("response %p\n", entry->response);
            char* ret = palloc_strdup(env, entry->response);
            pthread_rwlock_unlock(&lock); // Race here I guess
            DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
            return ret;
        }
    } while (NULL != (entry = entry->next));
    pthread_rwlock_unlock(&lock);
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
    return NULL;
}

void cache_delete (const char* request) {
    DEBUG("request=%s\n", request);

	struct cache_entry* prev = NULL;
	struct cache_entry* next = NULL;
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
    pthread_rwlock_wrlock(&lock);
	struct cache_entry* current = cache;

	while(current != NULL) {
		next = current->next;
		if (0 == strcmp(current->request, request)) {
			if (NULL == prev) {
                if (NULL == next) {
                    cache = palloc(*envp, struct cache_entry);
                    memset(cache, 0, sizeof(struct cache_entry));
                } else {
                    cache = next;
                }
			} else {
                prev->next = next;
			}
            pfree(current);
            // We're done, get out.
            current = NULL;
            num_entries--;
		} else {
			prev = current;
			current = current->next;
		}
	}
    pthread_rwlock_unlock(&lock); // ok if error
    DEBUG("readers=%d, writer=%d\n", lock.__data.__nr_readers, lock.__data.__writer);
}