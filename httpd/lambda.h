
/*
 * Copyright (C) 2011 Palmer Dabbelt
 *   <palmer@dabbelt.com>
 *
 * This file is part of pconfigure.
 * 
 * pconfigure is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pconfigure is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with pconfigure.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PCONFIGURE_LAMBDA_H
#define PCONFIGURE_LAMBDA_H

/* From http://walfield.org/blog/2010/08/25/lambdas-in-c.html */

/* Create a lambda function.  Note: unlike lambdas in functional
   languages, this lambda does not capture the containing
   environment.  Thus, if you access the enclosing environment, you
   must ensure that the lifetime of this lambda is bound by the
   lifetime of the enclosing environment (i.e., until the enclosing
   function returns).  This means that if you access local
   variables, bad things will happen.  If you don't access local
   variables, you're fine.  */
#define lambda(l_ret_type, l_arguments, l_body)         \
  ({                                                    \
    l_ret_type l_anonymous_functions_name l_arguments   \
      l_body                                            \
    &l_anonymous_functions_name;                        \
  })

#endif
