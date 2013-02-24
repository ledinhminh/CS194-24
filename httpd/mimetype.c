/* cs194-24 Lab 1 */

/* So I think we're supposed to define new mimetypes in here... 
 * fake polymorphism here we go.
 */

#define _BSD_SOURCE

#include "mimetype.h"
#include "mimetype_file.h"
#include "mimetype_cgi.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>

#ifndef HTTPD_ROOT
#define HTTPD_ROOT "/var/www"
#endif

void mimetype_init(struct mimetype *mt)
{
    memset(mt, 0, sizeof(*mt));
}

struct mimetype *mimetype_new(palloc_env env, const char *path)
{
    int fullpath_len;
    char *fullpath;
    struct mimetype *mt;
    
    int is_executable;

    fullpath_len = snprintf(NULL, 0, "%s/%s", HTTPD_ROOT, path) + 1;
    fullpath = palloc_array(env, char, fullpath_len);
    snprintf(fullpath, fullpath_len, "%s/%s", HTTPD_ROOT, path);
    
    // Now is the time.
    
    is_executable = access(fullpath, X_OK);
    INFO; printf("is_executable=%d\n", is_executable);
    
    if (0 == is_executable)
        mt = mimetype_cgi_new(env, fullpath);
    else
        mt = mimetype_file_new(env, fullpath);
    
    pfree(fullpath);

    return mt;
}
