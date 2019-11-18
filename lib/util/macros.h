/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#ifndef UTIL_MACROS_H__
#define UTIL_MACROS_H__

/* common macros are placed here, so as to avoid littering the code with
 * slightly different variations of them */

#ifndef MIN
#define MIN(x__,y__) \
    ((x__) < (y__) ? (x__) : (y__))
#endif

#ifndef MAX
#define MAX(x__,y__) \
    ((x__) > (y__) ? (x__) : (y__))
#endif

#define CLAMP(val__, low__, high__) \
    ((val__) < (low__) ? (low__) : ( (val__) > (high__) ? (high__) : (val__)))

#define ARRAY_SIZE(x__) (sizeof((x__))/sizeof((x__)[0]))

#define STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)

/* NULL sort order in *cmp funcs */
#define NULLCMP(l,r)                       \
  if ((l) == NULL && (r) == NULL) {        \
    return 0;                              \
  } else if ((l) == NULL && (r) != NULL) { \
    return 1;                              \
  } else if ((l) != NULL && (r) == NULL) { \
    return -1;                             \
  }

#endif
