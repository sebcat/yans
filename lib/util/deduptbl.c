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
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/sha.h>

#include <lib/util/macros.h>
#include <lib/util/io.h>
#include <lib/util/deduptbl.h>

#define DEDUPTBL_ID_EMPTY "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define TABLE_INDEX(tbl, val) \
    ((val) & ((1 << (tbl)->cap_log2) - 1))

static inline int deduptbl_error(struct deduptbl_ctx *ctx, int err) {
  ctx->saved_errno = errno;
  return err;
}

void deduptbl_id(struct deduptbl_id *id, const void *data, size_t len) {
  SHA_CTX c;

  STATIC_ASSERT(sizeof(id->val) == SHA_DIGEST_LENGTH, "invalid size");
  SHA1_Init(&c);
  SHA1_Update(&c, data, len);
  SHA1_Final(id->val.u8, &c);

  if (memcmp(id->val.u8, DEDUPTBL_ID_EMPTY, sizeof(id->val.u8)) == 0) {
    id->val.u32[0]++;
  }
}

void deduptbl_idv(struct deduptbl_id *id, struct deduptbl_vec *vec,
    size_t len) {
  SHA_CTX c;
  size_t i;

  STATIC_ASSERT(sizeof(id->val) == SHA_DIGEST_LENGTH, "invalid size");
  SHA1_Init(&c);
  for (i = 0; i < len; i++) {
    SHA1_Update(&c, vec[i].data, vec[i].len);
  }

  SHA1_Final(id->val.u8, &c);
  if (memcmp(id->val.u8, DEDUPTBL_ID_EMPTY, sizeof(id->val.u8)) == 0) {
    id->val.u32[0]++;
  }
}

const char *deduptbl_strerror(struct deduptbl_ctx *ctx, int err) {
  switch (err) {
  case DEDUPTBL_OK:
    return "no error";
  case DEDUPTBL_EERRNO:
    return strerror(ctx->saved_errno);
  case DEDUPTBL_ETOOBIG:
    return "table size too big";
  case DEDUPTBL_EMAGIC:
    return "invalid magic number in table";
  case DEDUPTBL_ESIZE:
    return "invalid table size";
  case DEDUPTBL_EFULL:
    return "table is full";
  case DEDUPTBL_EEXIST:
    return "entry already exist in table";
  case DEDUPTBL_ENOSLOT:
    return "no available slot in non-empty table";
  case DEDUPTBL_EHEADER:
    return "missing/invalid header";
  default:
    return "unknown error";
  }
}

/* portable version, from bit-twiddling hacks */
static inline uint32_t round_up_pow2(uint32_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

int deduptbl_create(struct deduptbl_ctx *ctx, uint32_t max_entries,
    int fd) {
  void *data;
  size_t cap;
  size_t cap_log2;
  size_t filesize;
  int ret;

  memset(ctx, 0, sizeof(*ctx));
  if (max_entries > DEDUPTBL_MAX_ENTRIES) {
    return deduptbl_error(ctx, DEDUPTBL_ETOOBIG);
  }

  /* calculate the table size */
  cap = round_up_pow2(MAX(max_entries + max_entries/6, 32));
  filesize =
      sizeof(struct deduptbl_table)+sizeof(struct deduptbl_entry)*((cap));

  /* allocate the table. If fd is non-negative, the resulting table will
   * be file-backed. Otherwise, it resides in memory only. */
  if (fd >= 0) {
    ret = ftruncate(fd, filesize);
    if (ret != 0) {
      return deduptbl_error(ctx, DEDUPTBL_EERRNO);
    }
    data = mmap(NULL, filesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  } else {
    data = mmap(NULL, filesize, PROT_READ|PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
  }
  if (data == MAP_FAILED) {
    return deduptbl_error(ctx, DEDUPTBL_EERRNO);
  }

  cap_log2 = 0;
  while (cap >>= 1) {
    cap_log2++;
  }

  ctx->table = data;
  ctx->filesize = filesize;
  ctx->table->magic = DEDUPTBL_MAGIC;
  ctx->table->nmax = max_entries;
  ctx->table->nused = 0;
  ctx->table->cap_log2 = cap_log2;
  return 0;
}

int deduptbl_load(struct deduptbl_ctx *ctx, int fd) {
  struct deduptbl_table tbl;
  struct stat sb;
  void *data;
  io_t io;
  int ret;

  IO_INIT(&io, fd);
  ret = io_readfull(&io, &tbl, sizeof(tbl));
  if (ret != IO_OK) {
    return deduptbl_error(ctx, DEDUPTBL_EHEADER);
  }

  if (tbl.magic != DEDUPTBL_MAGIC) {
    return deduptbl_error(ctx, DEDUPTBL_EMAGIC);
  }

  ret = fstat(fd, &sb);
  if (ret != 0) {
    return deduptbl_error(ctx, DEDUPTBL_EERRNO);
  }

  if (((1 << tbl.cap_log2)*sizeof(struct deduptbl_entry) +
      sizeof(struct deduptbl_table)) != sb.st_size) {
    return deduptbl_error(ctx, DEDUPTBL_ESIZE);
  }

  data = mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    return deduptbl_error(ctx, DEDUPTBL_EERRNO);
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->table = data;
  ctx->filesize = sb.st_size;
  return 0;
}

void deduptbl_cleanup(struct deduptbl_ctx *ctx) {
  if (ctx->table) {
    munmap(ctx->table, ctx->filesize);
    ctx->table = NULL;
  }
}

int deduptbl_contains(struct deduptbl_ctx *ctx, struct deduptbl_id *id) {
  uint32_t current_pos;
  uint32_t start_pos;
  uint32_t distance = 0;
  struct deduptbl_entry *ent;

  start_pos = TABLE_INDEX(ctx->table, id->val.u32[0]);
  current_pos = start_pos;
  do {
    ent = &ctx->table->entries[current_pos];
    if (memcmp(ent->id.val.u8, id->val.u8, sizeof(id->val.u8)) == 0) {
      return 1;
    } else if (ent->distance < distance ||
        memcmp(ent->id.val.u8, DEDUPTBL_ID_EMPTY,
        sizeof(id->val.u8)) == 0) {
      break;
    }
    current_pos = TABLE_INDEX(ctx->table, current_pos + 1);
    distance++;
  } while(current_pos != start_pos);

  return 0;
}

int deduptbl_update(struct deduptbl_ctx *ctx, struct deduptbl_id *id) {
  uint32_t start_pos;
  uint32_t current_pos;
  struct deduptbl_entry elem;
  struct deduptbl_entry tmp;
  struct deduptbl_entry *curr;
  struct deduptbl_table *tbl;

  tbl = ctx->table;
  if (ctx->table->nused >= tbl->nmax) {
    return DEDUPTBL_EFULL;
  }

  start_pos = TABLE_INDEX(tbl, id->val.u32[0]);
  current_pos = start_pos;

  /* initialize our element to insert */
  elem.id = *id;
  elem.distance = 0;

  do {
    curr = &tbl->entries[current_pos];
    if (memcmp(&curr->id, DEDUPTBL_ID_EMPTY, sizeof(curr->id)) == 0) {
      /* the current slot is empty - increment the variable holding the
       * number of entries in use and insert at this position */
      ctx->table->nused++;
      *curr = elem;
      return DEDUPTBL_OK;
    } else if (memcmp(&curr->id, id, sizeof(curr->id)) == 0) {
      /* the entry already exist in the table */
      return DEDUPTBL_EEXIST;
    } else if (curr->distance < elem.distance) {
      /* the distance of the current entry is less than that of the
       * to-be-inserted entry - swap 'em */
      tmp = *curr;
      *curr = elem;
      elem = tmp;
    }

    elem.distance++;
    current_pos = TABLE_INDEX(tbl, current_pos + 1);
  } while(current_pos != start_pos);

  /* shouldn't happen: unless the table is full (which is
   * checked for when calling the function) - insertion of a valid
   * key/value-pair should always be possible */
  return DEDUPTBL_ENOSLOT;
}

