/* cs194-24 Lab 1 */

#ifndef MIMETYPE_FILE_H
#define MIMETYPE_FILE_H

#include "mimetype.h"

struct mimetype_file
{
    union {
	struct mimetype;
	struct mimetype mimetype;
    };

    const char *fullpath;
};

struct mimetype *mimetype_file_new(palloc_env env, const char *fullpath);

#endif
