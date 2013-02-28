/* cs194-24 Lab 1 */

#ifndef DEBUG_H
#define DEBUG_H

#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

// There is a possibility it won't be used
static pthread_mutex_t __attribute__((unused)) __stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Prepend this to get some debugging information. */
#define INFO pthread_mutex_lock(&__stdout_mutex); printf("[%x/%x] %s:%d(%s): ", (unsigned int) pthread_self(), syscall(SYS_gettid), __FILE__, __LINE__, __func__); pthread_mutex_unlock(&__stdout_mutex);

#define DEBUG(...) \
    do { \
        pthread_mutex_lock(&__stdout_mutex); \
        printf("(thread %lu) %s:%d(%s): ", syscall(SYS_gettid), __FILE__, __LINE__, __func__); \
        printf(__VA_ARGS__); \
        pthread_mutex_unlock(&__stdout_mutex); \
    } while (0);

/* Allocates *str in the palloc_env env and fills it with the rest of arguments to snprintf. */
#define psnprintf(str, env, ...) \
    do { \
        int len = snprintf(NULL, 0, __VA_ARGS__) + 1; \
        str = palloc_array(env, char, len); \
        snprintf(str, len, __VA_ARGS__); \
    } while (0); 

#endif
