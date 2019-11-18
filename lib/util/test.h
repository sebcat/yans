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
#ifndef UTIL_TEST_H__
#define UTIL_TEST_H__
#include <stdlib.h>
#include <stdio.h> 

#define TEST_OK 42
#define TEST_FAIL -1

/*NB: str,fmt must be a string literal */
#define TEST_LOG(str) \
    fprintf(stderr, "  %s:%d %s\n", __FILE__, __LINE__, str)
#define TEST_LOGF(fmt, ...) \
    fprintf(stderr, "  %s:%d " fmt "\n", __FILE__, __LINE__, __VA_ARGS__)

#define TEST_ENTRY(...) \
int main() { \
  int ret = EXIT_SUCCESS; \
  size_t i; \
  struct { \
    char *name; \
    int (*func)(void); \
  } tests[] = { \
    __VA_ARGS__ \
    {NULL, NULL}, \
  }; \
  for (i = 0; tests[i].name != NULL; i++) { \
    if (tests[i].func() == TEST_OK) { \
      fprintf(stderr, "OK  %s\n", tests[i].name); \
    } else { \
      fprintf(stderr, "ERR %s\n", tests[i].name); \
      ret = EXIT_FAILURE; \
    } \
  } \
  return ret; \
}

#endif
