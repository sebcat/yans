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
#include <stdbool.h>
#include <string.h>

#include <lib/util/str.h>

static const char *in_set(const char *s, size_t len, char ch) {
  size_t i;

  for (i = 0; i < len; i++) {
    if (s[i] == ch) {
      return s + i;
    }
  }

  return NULL;
}

int str_map_field(const char *s, size_t len,
    const char *seps, size_t sepslen,
    int (*func)(const char *, size_t, void *),
    void *data) {
  size_t i;
  size_t start = 0;
  bool in_str = false;
  int ret;
  char ch;

  /* two-state/boolean FSM with the current character as input */
  for (i = 0; i < len; i++) {
    ch = s[i];
    if (in_set(seps, sepslen, ch)) {
      if (in_str) {
        ret = func(s + start, i - start, data);
        if (ret <= 0) {
          return ret;
        }
        in_str = !in_str;
      }
    } else {
      if (!in_str) {
        start = i;
        in_str = !in_str;
      }
    }
  }

  /* handle trailing field w/ no trailing field separator */
  if (in_str && start < i) {
    return func(s + start, i - start, data);
  }

  return 0;
}

int str_map_fieldz(const char *s, const char *seps,
    int (*func)(const char *, size_t, void *),
    void *data) {
  size_t i;
  size_t start = 0;
  bool in_str = false;
  int ret;
  char ch;

  /* two-state/boolean FSM with the current character as input */
  for (i = 0; (ch = s[i]) != '\0'; i++) {
    if (strchr(seps, ch)) {
      if (in_str) {
        ret = func(s + start, i - start, data);
        if (ret <= 0) {
          return ret;
        }
        in_str = !in_str;
      }
    } else {
      if (!in_str) {
        start = i;
        in_str = !in_str;
      }
    }
  }

  /* handle trailing field w/ no trailing field separator */
  if (in_str && start < i) {
    return func(s + start, i - start, data);
  }

  return 0;
}

