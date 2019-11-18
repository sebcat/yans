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
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <lib/util/flagset.h>

int flagset_from_str(const struct flagset_map *m, const char *s,
    struct flagset_result *out) {
  size_t len;
  size_t i;
  unsigned int res = 0;
  const char *start = s;

  memset(out, 0, sizeof(*out));
  if (s == NULL || *s == '\0') {
    return 0;
  }

  for(s += strspn(s, FLAGSET_SEPS); *s != '\0'; s += strspn(s, FLAGSET_SEPS)) {
    /* determine the length of the current flag, error out if it's too long */
    len = strcspn(s, FLAGSET_SEPS);
    if (len > FLAGSET_MAXSZ) {
      out->errmsg = "flag too long";
      out->erroff = s - start;
      return -1;
    }

    /* N is small, do a linear search for the flag */
    for (i = 0; m[i].name != NULL; i++) {
      if (len == m[i].namelen && memcmp(m[i].name, s, len) == 0) {
        res |= m[i].flag;
        break;
      }
    }

    /* report an error if the flag is not in the set */
    if (m[i].name == NULL) {
      out->errmsg = "flag does not exist";
      out->erroff = s - start;
      return -1;
    }

    s += len;
  }

  out->flags = res;
  return 0;
}

