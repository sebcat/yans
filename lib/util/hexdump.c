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

#include "lib/util/hexdump.h"

int hexdump(FILE *fp, void *data, size_t len) {
  size_t i;
  size_t mod;
  const unsigned char *cptr = data;

  for (i = 0; i < len; i++) {
    mod = i & 0x0f;
    if (mod == 0) {
      fprintf(fp, "%8zx    ", i);
    }

    if (mod == 15) {
      fprintf(fp, "%.2x\n", (unsigned int)cptr[i]);
    } else {
      fprintf(fp, "%.2x ", (unsigned int)cptr[i]);
    }
  }

  if (ferror(fp)) {
    return -1;
  }

  return 0;
}
