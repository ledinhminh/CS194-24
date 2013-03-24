/* cs194-24 Lab 1 */

#ifndef MIMETYPE_H
#define MIMETYPE_H

#include "palloc.h"
#include "http.h"

struct mimetype
{
    int (*http_get)(struct mimetype *, struct http_session *);
};

void mimetype_init(struct mimetype *mt);

struct mimetype *mimetype_new(palloc_env env, const char *path);

#endif
