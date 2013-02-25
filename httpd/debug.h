/* cs194-24 Lab 1 */

#ifndef DEBUG_H
#define DEBUG_H

/* Prepend this to get some debugging information. */
#define INFO printf("%s:%d(%s): ", __FILE__, __LINE__, __func__)

/* Allocates *str in the palloc_env env and fills it with the rest of arguments to snprintf. */
#define psnprintf(str, env, ...) \
    do { \
        int len = snprintf(NULL, 0, __VA_ARGS__) + 1; \
        str = palloc_array(env, char, len); \
        snprintf(str, len, __VA_ARGS__); \
    } while (0); 

#endif
