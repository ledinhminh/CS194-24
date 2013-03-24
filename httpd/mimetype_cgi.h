/* cs194-24 Lab 1 */

#ifndef MIMETYPE_CGI_H
#define MIMETYPE_CGI_H

#include "mimetype.h"

struct mimetype_cgi {
  union {
    struct mimetype;
    struct mimetype mimetype;
  };

  const char *fullpath;
};

struct mimetype *mimetype_cgi_new(palloc_env env, const char *fullpath);

#endif
