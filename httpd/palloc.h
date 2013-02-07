/* cs194-24 Lab 1 */

/* 
   Unix SMB/CIFS implementation.
   Samba temporary memory allocation functions

   Copyright (C) Andrew Tridgell 2004-2005
   Copyright (C) Stefan Metzmacher 2006
   
     ** NOTE! The following LGPL license applies to the palloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PALLOC_H
#define PALLOC_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

typedef void *palloc_env;

/**
 * @brief Create a new top level palloc context.
 *
 * This function creates a zero length named palloc context as a top
 * level context.
 *
 * @param[in]  fmt      Format string for the name.
 *
 * @param[in]  ...      Additional printf-style arguments.
 *
 * @return              The allocated memory chunk, NULL on error.
 */
palloc_env palloc_init(const char *format, ...) __attribute__ ((format (__printf__, 1, 2)));

/**
 * @brief Change the size of a palloc array.
 *
 * The macro changes the size of a palloc pointer. The 'count'
 * argument is the number of elements of type 'type' that you want the
 * resulting pointer to hold.
 *
 *
 * @param[in]  ptr      The chunk to be resized.
 *
 * @param[in]  size     The intended new block size.
 *
 * @return              The new array, NULL on error.
 */
void *prealloc(const void *ptr, size_t size);

/**
 * @brief Create a new palloc context.
 *
 * The palloc() macro is the core of the palloc library. It takes a
 * memory context and a type, and returns a pointer to a new area of
 * memory of the given type.
 *
 * The returned pointer is itself a palloc context, so you can use it
 * as the context argument to more calls to palloc if you wish.
 *
 * The returned pointer is a "child" of the supplied context. This
 * means that if you palloc_free() the context then the new child
 * disappears as well.  Alternatively you can free just the child.
 *
 * @param[in]  env      A palloc context to create a new reference.
 *
 * @param[in]  type     The type of memory to allocate.
 *
 * @return              A type casted palloc context or NULL on error.
 */
#define palloc(env, type) (type *)_palloc(env, sizeof(type), #type)
void *_palloc(palloc_env env, size_t size, const char *type);

/**
 * @brief Allocate an array.
 *
 * The macro is equivalent to:
 *
 * @code
 *      (type *)palloc_size(ctx, sizeof(type) * count);
 * @endcode
 *
 * except that it provides integer overflow protection for the
 * multiply, returning NULL if the multiply overflows.
 *
 * @param[in]  ctx      The palloc context to hang the result off.
 *
 * @param[in]  type     The type that we want to allocate.
 *
 * @param[in]  count    The number of 'type' elements you want to allocate.
 *
 * @return              The allocated result, properly cast to 'type *', NULL on
 *                      error.
 */
#define palloc_array(env, t, n) (t *)_palloc(env, sizeof(t) * n, #t)

/**
 * @brief Free a chunk of palloc memory.
 *
 * The palloc_free() function frees a piece of palloc memory, and all
 * its children. You can call palloc_free() on any pointer returned by
 * palloc().
 *
 * The return value of palloc_free() indicates success or failure,
 * with 0 returned for success and -1 for failure. A possible failure
 * condition is if the pointer had a destructor attached to it and the
 * destructor returned -1. See palloc_destructor() for details on
 * destructors. Likewise, if "ptr" is NULL, then the function will
 * make no modifications and return -1.
 *
 * palloc_free() operates recursively on its children.
 *
 * @param[in]  ptr      The chunk to be freed.
 *
 * @return              Returns 0 on success and -1 on error.
 */
int pfree(const void *ptr);

/**
 * @brief Get a typed pointer out of a palloc pointer.
 *
 * This macro allows you to do type checking on palloc pointers. It is
 * particularly useful for void* private pointers.
 *
 * @param[in]  ptr      The palloc pointer to check.
 *
 * @param[in]  type     The type to check against.
 *
 * @return              The properly casted pointer given by ptr, NULL on error.
 */
#define palloc_cast(ptr, type) (type *)_palloc_cast(ptr, #type)
void * _palloc_cast(const void *ptr, const char *type);

/**
 * @brief Assign a destructor function to be called when a chunk is freed.
 *
 * The function palloc_set_destructor() sets the "destructor" for the
 * pointer "ptr". A destructor is a function that is called when the
 * memory used by a pointer is about to be released. The destructor
 * receives the pointer as an argument, and should return 0 for
 * success and -1 for failure.
 *
 * The destructor can do anything it wants to, including freeing other
 * pieces of memory. A common use for destructors is to clean up
 * operating system resources (such as open file descriptors)
 * contained in the structure the destructor is placed on.
 *
 * You can only place one destructor on a pointer. If you need more
 * than one destructor then you can create a zero-length child of the
 * pointer and place an additional destructor on that.
 *
 * To remove a destructor call palloc_set_destructor() with NULL for
 * the destructor.
 *
 * @param[in]  ptr      The palloc chunk to add a destructor to.
 *
 * @param[in]  desc     The destructor function to be called. NULL to remove
 *                      it.
 */
#define palloc_destructor(ptr, desc)					\
    do {								\
	int (*d)(__typeof__(ptr)) = (desc);				\
	_palloc_destructor(ptr, (int (*)(void *))d);			\
    } while (0)
void _palloc_destructor(const void *ptr, int (*dest)(void *));

/**
 * @brief Duplicate a string into a palloc chunk.
 *
 * @param[in]  env      The palloc context to hang the result off.
 *
 * @param[in]  str      The string you want to duplicate.
 *
 * @return              The duplicated string, NULL on error.
 */
char *palloc_strdup(palloc_env env, const char *str);

void palloc_print_tree(const void *ptr);

#endif
