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
#ifndef YANS_DEDUPTBL_H__
#define YANS_DEDUPTBL_H__

#include <stddef.h>
#include <stdint.h>

#define DEDUPTBL_MAGIC 0x75646564

#define DEDUPTBL_MAX_ENTRIES 0x3fffffff

/* return values */
#define DEDUPTBL_OK       0
#define DEDUPTBL_EERRNO  -1
#define DEDUPTBL_ETOOBIG -2
#define DEDUPTBL_EMAGIC  -3
#define DEDUPTBL_ESIZE   -4
#define DEDUPTBL_EFULL   -5
#define DEDUPTBL_EEXIST  -6
#define DEDUPTBL_ENOSLOT -7
#define DEDUPTBL_EHEADER -8

struct deduptbl_id {
  union {
    uint8_t u8[20];
    uint32_t u32[5];
  } val;
};

struct deduptbl_entry {
  uint32_t distance;
  struct deduptbl_id id;
};

struct deduptbl_table {
  uint32_t magic;
  uint32_t nmax;      /* maximum number of entries allowed in the table */
  uint32_t nused;     /* number of entries in use */
  uint32_t cap_log2;  /* log2 of number of slots in the table */
  struct deduptbl_entry entries[];
};

struct deduptbl_ctx {
  struct deduptbl_table *table;
  size_t filesize;
  int saved_errno;
};

struct deduptbl_vec {
  const void *data;
  size_t len;
};

void deduptbl_id(struct deduptbl_id *id, const void *data, size_t len);
void deduptbl_idv(struct deduptbl_id *id, struct deduptbl_vec *vec,
    size_t len);

const char *deduptbl_strerror(struct deduptbl_ctx *ctx, int err);

int deduptbl_create(struct deduptbl_ctx *ctx, uint32_t max_entries,
    int fd);
int deduptbl_load(struct deduptbl_ctx *ctx, int fd);
void deduptbl_cleanup(struct deduptbl_ctx *ctx);
int deduptbl_contains(struct deduptbl_ctx *ctx, struct deduptbl_id *id);
int deduptbl_update(struct deduptbl_ctx *ctx, struct deduptbl_id *id);


#endif
