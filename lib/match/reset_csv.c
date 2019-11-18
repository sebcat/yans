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
#include <stdio.h>
#include <stdlib.h>

#include <lib/util/csv.h>
#include <lib/match/reset.h>

int reset_csv_load(struct reset_t *reset, FILE *in, size_t *npatterns) {
  struct csv_reader reader;
  int ret;
  int status = RESET_OK;
  const char *type;
  const char *name;
  const char *pattern;
  size_t nloaded = 0;
  char *cptr;
  enum reset_match_type mtype;

  ret = csv_reader_init(&reader);
  if (ret < 0) {
    return reset_error(reset, "failed to initialize CSV reader");
  }

  /* NB: This is best-effort - There may be empty lines in the CSV,
         it is likely to have column names on the first row, &c. We
         just ignore row parse errors and communicate number of patterns
         added using npatterns instead. We fail on invalid patterns */
  while (!feof(in)) {
    ret = csv_read_row(&reader, in);
    if (ret < 0) {
      status = reset_error(reset, "read failure");
      break;
    }

    type = csv_reader_elem(&reader, 0);
    name = csv_reader_elem(&reader, 1);
    pattern = csv_reader_elem(&reader, 2);
    if (!type || !name || !pattern) {
      continue;
    }

    mtype = (int)strtol(type, &cptr, 10);
    if (*cptr != '\0') {
      continue;
    }

    ret = reset_add_type_name_pattern(reset, mtype, name, pattern);
    if (ret == RESET_ERR) {
      char errbuf[256];
      snprintf(errbuf, sizeof(errbuf), "%s: %s",
          reset_strerror(reset), pattern);
      status = reset_error(reset, errbuf);
      break;
    }

    nloaded++;
  }

  if (npatterns) {
    *npatterns = nloaded;
  }

  csv_reader_cleanup(&reader);
  return status;
}
