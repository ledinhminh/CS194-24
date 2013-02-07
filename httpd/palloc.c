/* cs194-24 Lab 1 */

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "palloc.h"

struct child_list
{
    struct child_list *next;
    struct block *blk;
};

struct block
{
    struct block *parent;
    struct child_list *children;
    const char *pool_name;
    const char *type;
    int (*destructor)(void *ptr);
};

static inline struct block *ENV_BLK(const palloc_env env)
{
    return (struct block *)(((char *)env) - sizeof(struct block));
}

static inline struct block *PTR_BLK(const void *ptr)
{
    return (struct block *)(((char *)ptr) - sizeof(struct block));
}

static inline palloc_env BLK_ENV(const struct block *blk)
{
    return (palloc_env)(((char *)blk) + sizeof(struct block));
}

static inline void *BLK_PTR(const struct block *blk)
{
    return (void *)(((char *)blk) + sizeof(struct block));
}

static struct block *block_new(int size);

static int _pfree(const void *ptr, bool external);

static void _palloc_print_tree(struct block *blk, int level);

palloc_env palloc_init(const char *format, ...)
{
    va_list args;
    struct block *blk;
    char *pool_name;

    blk = block_new(0);
    if (blk == NULL)
	return NULL;

    va_start(args, format);
    vasprintf(&pool_name, format, args);
    va_end(args);

    blk->pool_name = pool_name;

    return BLK_ENV(blk);
}

void *prealloc(const void *ptr, size_t size)
{
    struct block *blk, *nblk;

    blk = PTR_BLK(ptr);
    nblk = realloc(blk, size + sizeof(*blk));

    if (nblk == NULL)
    {
	pfree(blk);
	return NULL;
    }

    if (nblk != blk)
    {
	struct child_list *cur;

	cur = nblk->parent->children;
	while (cur != NULL)
	{
	    if (cur->blk == blk)
		cur->blk = nblk;

	    cur = cur->next;
	}
    }

    return BLK_PTR(nblk);
}

void *_palloc(palloc_env env, size_t size, const char *type)
{
    struct block *blk;
    struct block *cblk;
    struct child_list *cnode;

    blk = ENV_BLK(env);
    
    cblk = block_new(size);
    if (cblk == NULL)
	return NULL;

    cnode = malloc(sizeof(*cnode));
    if (cnode == NULL)
    {
	free(cblk);
	return NULL;
    }

    cblk->parent = blk;
    cblk->type = type;
    cnode->blk = cblk;
    cnode->next = blk->children;
    blk->children = cnode;

    return BLK_ENV(cblk);
}

int pfree(const void *ptr)
{
    return _pfree(ptr, true);
}

void * _palloc_cast(const void *ptr, const char *type)
{
    struct block *blk;

    blk = PTR_BLK(ptr);

    if (blk->type != type)
	return NULL;

    return (void *)ptr;
}

char *palloc_strdup(palloc_env env, const char *str)
{
    char *out;

    out = palloc_array(env, char, strlen(str) + 2);
    strcpy(out, str);

    return out;
}

void _palloc_destructor(const void *ptr, int (*dest)(void *))
{
    struct block *blk;

    blk = PTR_BLK(ptr);
    blk->destructor = dest;
}

void palloc_print_tree(const void *ptr)
{
    return _palloc_print_tree(PTR_BLK(ptr), 0);
}

struct block *block_new(int size)
{
    struct block *b;

    b = malloc(sizeof(*b) + size);
    if (b == NULL)
	return NULL;

    b->parent = NULL;
    b->children = NULL;
    b->pool_name = NULL;
    b->type = NULL;
    b->destructor = NULL;

    return b;
}

int _pfree(const void *ptr, bool external)
{
    struct block *blk;
    struct child_list *cur, *prev;
    int ret;

    blk = PTR_BLK(ptr);

    ret = 0;
    cur = blk->children;
    while (cur != NULL)
    {
	struct child_list *next;

	ret |= _pfree(BLK_ENV(cur->blk), false);

	next = cur->next;
	free(cur);
	cur = next;
    }

    if (blk->pool_name != NULL)
	free((char *)blk->pool_name);

    if (blk->parent != NULL && external)
    {
	prev = NULL;
	cur = blk->parent->children;
	while (cur != NULL && cur->blk != blk)
	    cur = cur->next;

	if (cur == NULL)
	{
	    fprintf(stderr, "Inconsistant palloc tree during pfree()\n");
	    abort();
	}
	else if (prev == NULL)
	{
	    blk->parent->children = cur->next;
	    free(cur);
	}
	else
	{
	    prev->next = cur->next;
	    free(cur);
	}
    }

    if (blk->destructor != NULL)
	blk->destructor(BLK_PTR(blk));

    free(blk);

    return ret;
}

void _palloc_print_tree(struct block *blk, int level)
{
    struct child_list *cur;
    const char *string;

    string = (blk->pool_name == NULL) ? blk->type : blk->pool_name;
    printf("%*s%s\n", level, "", string);

    for (cur = blk->children; cur != NULL; cur = cur->next)
	_palloc_print_tree(cur->blk, level+1);
}
