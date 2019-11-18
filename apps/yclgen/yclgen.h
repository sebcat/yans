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
#ifndef YCLGEN_H__
#define YCLGEN_H__

#include <stdio.h>

#define MAX_NAMELEN 32
#define MAX_FIELDS 32

#define MSGF_HASDARR (1 << 0)
#define MSGF_HASLARR (1 << 1)
#define MSGF_HASLONG (1 << 2)
#define MSGF_HASDATA (1 << 3)

/* intdicated that the first element is a struct */
#define MSGF_HASNEST (1 << 4)

enum yclgen_field_type {
  FT_DATAARR,
  FT_LONGARR,
  FT_DATA,
  FT_LONG,
};

struct yclgen_field {
  enum yclgen_field_type typ;
  char name[MAX_NAMELEN];
};

struct yclgen_msg {
  struct yclgen_msg *next;
  int flags;
  char name[MAX_NAMELEN];
  int nfields;
  struct yclgen_field fields[MAX_FIELDS];
};

struct yclgen_ctx {
  struct yclgen_msg *latest_msg;
  int flags;
};

int yclgen_parse(struct yclgen_ctx *ctx, FILE *fp);
void yclgen_cleanup(struct yclgen_ctx *ctx);

#endif /* YCLGEN_H__ */
