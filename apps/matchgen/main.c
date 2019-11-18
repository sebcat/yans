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
#include <ctype.h>

#include <lib/match/reset.h>
#include <lib/util/csv.h>

static void emit_str(FILE *out, const char *s) {
  int ch;

  fputc('"', out);
  while ((ch = *s++) != '\0') {
    if (ch == '\\') {
      fprintf(out, "\\\\");
    } else if (ch == '"') {
      fprintf(out, "\\\"");
    } else if (isprint(ch)) {
      fputc(ch, out);
    } else {
      fprintf(out, "\\x%02x", ch);
    }
  }

  fputc('"', out);
}

static int emit_elems(FILE *out, FILE *in) {
  int ret;
  struct csv_reader reader;
  int status = -1;
  const char *type;
  const char *name;
  const char *pattern;
  char *cptr;
  enum reset_match_type mtype;

  ret = csv_reader_init(&reader);
  if (ret < 0) {
    fprintf(stderr, "csv_reader_init failure\n");
    return -1;
  }

  while (!feof(in)) {
    ret = csv_read_row(&reader, in);
    if (ret < 0) {
      fprintf(stderr, "csv_read_row failure\n");
      goto done;
    }

    type = csv_reader_elem(&reader, 0);
    name = csv_reader_elem(&reader, 1);
    pattern = csv_reader_elem(&reader, 2);
    if (!type || !name || !pattern) {
      continue;
    }

    mtype = (int)strtol(type, &cptr, 10);
    if (*cptr != '\0' || mtype <= RESET_MATCH_UNKNOWN ||
        mtype >= RESET_MATCH_MAX) {
      continue;
    }

    fprintf(out, "  {\n    .type    = %s,\n    .name    = ",
        reset_type2str(mtype));
    emit_str(out, name);
    fprintf(out,",\n    .pattern = ");
    emit_str(out, pattern);
    fprintf(out, ",\n  },\n");
  }

  status = 0;
done:
  csv_reader_cleanup(&reader);
  return status;
}


int main(int argc, char *argv[]) {
  int ret;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <varname>\n", argv[0]);
    return EXIT_FAILURE;
  }

  fprintf(stdout, "static const struct reset_pattern %s[] = {\n",
      argv[1]);
  ret = emit_elems(stdout, stdin);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  fprintf(stdout, "};\n");

  return EXIT_SUCCESS;
}
