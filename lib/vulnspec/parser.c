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
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "lib/util/macros.h"
#include "lib/vulnspec/vulnspec.h"

/* dereference a struct vulnspec_value */
#define NOD(p, v) ((void*)((p)->progn.buf.data + (v).offset))

/* initial size of string table */
#define STRTAB_ENTRIES 4096

struct strtab_entry {
  char *str;
  size_t len;
  struct vulnspec_value val;
};

static int strtab_cmp(const void *k, const void *e) {
  const struct strtab_entry *key = k;
  const struct strtab_entry *entry = e;
  size_t minlen;

  /* NB: comparison includes trailing \0-byte */
  minlen = MIN(key->len, entry->len);
  return memcmp(key->str, entry->str, minlen);
}

static objtbl_hash_t strtab_hash(const void *obj, objtbl_hash_t seed) {
  objtbl_hash_t h;
  const struct strtab_entry *entry = obj;
  h = objtbl_strhash(entry->str, seed);
  return h;
}

static struct strtab_entry *strtab_new(const char *s, size_t len,
    struct vulnspec_value val) {
  struct strtab_entry *e;

  e = calloc(1, sizeof(struct strtab_entry));
  if (e == NULL) {
    return NULL;
  }

  e->str = strdup(s);
  if (e->str == NULL) {
    free(e);
    return NULL;
  }

  e->len = len;
  e->val = val;
  return e;
}

static void strtab_free(struct strtab_entry *ent) {
  if (ent) {
    free(ent->str);
    free(ent);
  }
}

static void progn_alloc(struct vulnspec_parser *p, size_t len,
    struct vulnspec_value *out) {
  int ret;

  ret = vulnspec_progn_alloc(&p->progn, len, out);
  if (ret != 0) {
    longjmp(p->errjmp, VULNSPEC_EMALLOC);
  }
}

static inline void bail(struct vulnspec_parser *p, int err) {
  longjmp(p->errjmp, err);
}

static inline void expect(struct vulnspec_parser *p, enum vulnspec_token t) {
  if (vulnspec_read_token(&p->r) != t) {
    bail(p, VULNSPEC_EUNEXPECTED_TOKEN);
  }
}

static void loads(struct vulnspec_parser *p,
    struct vulnspec_cvalue *out) {
  struct vulnspec_value val;
  int ret;
  void *tblentry;
  struct strtab_entry e;

  /* load the string from the reader */
  expect(p, VULNSPEC_TSTRING);
  e.str = (char*)vulnspec_reader_string(&p->r, &e.len);
  e.len++; /* include trailing null byte */

  /* lookup the string in the string table. If the string is already added,
   * reuse that offset. Otherwise, add the string and save the offset in
   * the string table */
  ret = objtbl_get(&p->strtab, &e, &tblentry);
  if (ret == OBJTBL_OK) {
    struct strtab_entry *ent = tblentry;
    out->length = ent->len;
    out->value = ent->val;
  } else if (ret == OBJTBL_ENOTFOUND) {
    struct strtab_entry *ent;
    progn_alloc(p, e.len, &val);
    memcpy(NOD(p, val), e.str, e.len);
    ent = strtab_new(e.str, e.len, val);
    if (ent == NULL) {
      bail(p, VULNSPEC_ESTRTAB);
    }

    ret = objtbl_insert(&p->strtab, ent);
    if (ret != OBJTBL_OK) {
      strtab_free(ent);
      bail(p, VULNSPEC_ESTRTAB);
    }

    out->length = ent->len;
    out->value = ent->val;
  } else {
    bail(p, VULNSPEC_ESTRTAB);
  }
}

static enum vulnspec_node_type symbol2node(const char *sym) {
  size_t i;
  struct {
    const char *symbol;
    enum vulnspec_node_type node;
  } vals[] = {
    {"v",  VULNSPEC_OR_NODE},
    {"^",  VULNSPEC_AND_NODE},
    {"<",  VULNSPEC_LT_NODE},
    {"<=", VULNSPEC_LE_NODE},
    {"=",  VULNSPEC_EQ_NODE},
    {">=", VULNSPEC_GE_NODE},
    {">", VULNSPEC_GT_NODE},
    {"cve", VULNSPEC_CVE_NODE},
    {"nalpha", VULNSPEC_NALPHA_NODE},
  };

  for (i = 0; i < ARRAY_SIZE(vals); i++) {
    if (strcmp(sym, vals[i].symbol) == 0) {
      return vals[i].node;
    }
  }

  return VULNSPEC_INVALID_NODE;
}

static enum vulnspec_node_type read_symbol(struct vulnspec_parser *p) {
  enum vulnspec_token t;
  const char *sym;

  t = vulnspec_read_token(&p->r);
  if (t != VULNSPEC_TSYMBOL) {
    bail(p, VULNSPEC_EUNEXPECTED_TOKEN);
  }

  sym = vulnspec_reader_symbol(&p->r);
  return symbol2node(sym);
}

static struct vulnspec_value compar(struct vulnspec_parser *p,
    enum vulnspec_node_type t) {
  struct vulnspec_value compar;
  struct vulnspec_cvalue cvendprod;
  struct vulnspec_cvalue cversion;
  const char *versionstr = NULL;

  progn_alloc(p, sizeof(struct vulnspec_compar_node), &compar);

  loads(p, &cvendprod); /* load vend/prod string */

  if (p->vtype == VULNSPEC_VVAGUE) {
    /* load the version into the reader buffer but do not allocate
     * a string for it in the string table */
    expect(p, VULNSPEC_TSTRING); /* version */
    versionstr = (char*)vulnspec_reader_string(&p->r, NULL);
  } else {
    /* allocate a version string in the string table */
    loads(p, &cversion);
  }

  do {
    struct vulnspec_compar_node *node = NOD(p, compar);
    node->type = t;
    node->vtype = p->vtype;
    node->vendprod = cvendprod;
    if (p->vtype == VULNSPEC_VVAGUE) {
      vaguever_init(&node->version.vague, versionstr);
    } else {
      node->version.cval = cversion;
    }
  } while(0);

  expect(p, VULNSPEC_TRPAREN);
  return compar;
}

static struct vulnspec_value vulnexpr(struct vulnspec_parser *p);

static struct vulnspec_value boolean(struct vulnspec_parser *p,
    enum vulnspec_node_type nodet) {
  struct vulnspec_value bnode = {0};
  struct vulnspec_value curr;
  struct vulnspec_value prev;
  struct vulnspec_value val;
  enum vulnspec_token tok;

  prev.offset = 0;
  curr = bnode;
  while ((tok = vulnspec_read_token(&p->r)) == VULNSPEC_TLPAREN) {
    progn_alloc(p, sizeof(struct vulnspec_boolean_node), &curr);
    if (bnode.offset == 0) {
      bnode = curr; /* save first entry */
    }

    val = vulnexpr(p);
    do {
      struct vulnspec_boolean_node *node = NOD(p, curr);
      node->type = nodet;
      node->value = val;
    } while(0);

    if (prev.offset != 0) {
      do {
        struct vulnspec_boolean_node *node = NOD(p, prev);
        node->next = curr;
      } while(0);

    }

    prev = curr;
  }

  if (tok != VULNSPEC_TRPAREN) {
    bail(p, VULNSPEC_EUNEXPECTED_TOKEN);
  }

  if (bnode.offset == 0) {
    bail(p, VULNSPEC_EUNEXPECTED_TOKEN);
  }

  return bnode;
}

static struct vulnspec_value nalpha(struct vulnspec_parser *p) {
  struct vulnspec_value val = {0};
  enum vulnspec_version_type tmp;

  tmp = p->vtype;
  p->vtype = VULNSPEC_VNALPHA;
  expect(p, VULNSPEC_TLPAREN);
  val = vulnexpr(p);
  expect(p, VULNSPEC_TRPAREN);
  p->vtype = tmp;
  return val;
}

static struct vulnspec_value vulnexpr(struct vulnspec_parser *p) {
  struct vulnspec_value val = {0};
  enum vulnspec_node_type nodet;

  nodet = read_symbol(p);
  switch (nodet) {
  case VULNSPEC_LT_NODE:
  case VULNSPEC_LE_NODE:
  case VULNSPEC_EQ_NODE:
  case VULNSPEC_GE_NODE:
  case VULNSPEC_GT_NODE:
    val = compar(p, nodet);
    break;
  case VULNSPEC_AND_NODE:
  case VULNSPEC_OR_NODE:
    val = boolean(p, nodet);
    break;
  case VULNSPEC_NALPHA_NODE:
    val = nalpha(p);
    break;
  default:
    bail(p, VULNSPEC_EUNEXPECTED_TOKEN);
    break;
  }

  return val;
}

static struct vulnspec_value cve(struct vulnspec_parser *p) {
  struct vulnspec_value cve;
  struct vulnspec_value vexpr;
  struct vulnspec_cvalue cval;

  progn_alloc(p, sizeof(struct vulnspec_cve_node), &cve);
  loads(p, &cval);
  do {
    struct vulnspec_cve_node *node = NOD(p, cve);
    node->id = cval;
    expect(p, VULNSPEC_TDOUBLE);
    node->cvss2_base = (uint32_t)(vulnspec_reader_double(&p->r) * 100.0);
    expect(p, VULNSPEC_TDOUBLE);
    node->cvss3_base = (uint32_t)(vulnspec_reader_double(&p->r) * 100.0);
    node->type = VULNSPEC_CVE_NODE;
  } while(0);

  loads(p, &cval);
  do {
    struct vulnspec_cve_node *node = NOD(p, cve);
    node->description = cval;
  } while(0);

  expect(p, VULNSPEC_TLPAREN);
  vexpr = vulnexpr(p);
  do {
    struct vulnspec_cve_node *node = NOD(p, cve);
    node->vulnexpr = vexpr;
  } while(0);

  return cve;
}

int vulnspec_parser_init(struct vulnspec_parser *p) {
  int ret;
  static const struct objtbl_opts strtab_opts = {
    .hashfunc = strtab_hash,
    .cmpfunc  = strtab_cmp,
  };

  memset(p, 0, sizeof(*p));
  ret = objtbl_init(&p->strtab, &strtab_opts, STRTAB_ENTRIES);
  if (ret != OBJTBL_OK) {
    return -1;
  }

  return vulnspec_progn_init(&p->progn);
}

static int free_strtab(void *data, void *elem) {
  strtab_free(elem);
  return 1;
}

void vulnspec_parser_cleanup(struct vulnspec_parser *p) {
  objtbl_foreach(&p->strtab, free_strtab, NULL);
  objtbl_cleanup(&p->strtab);
  vulnspec_progn_cleanup(&p->progn);
}

int vulnspec_parse(struct vulnspec_parser *p, FILE *in) {
  enum vulnspec_token tok;
  enum vulnspec_node_type nodet;
  struct vulnspec_value curr;
  struct vulnspec_value prev;
  int status;

  status = vulnspec_reader_init(&p->r, in);
  if (status != 0) {
    return VULNSPEC_EMALLOC;
  }

  buf_clear(&p->progn.buf);
  buf_adata(&p->progn.buf, VULNSPEC_HEADER, VULNSPEC_HEADER_SIZE);
  if ((status = setjmp(p->errjmp)) != 0) {
    goto done;
  }

  prev.offset = 0;
  while ((tok = vulnspec_read_token(&p->r)) != VULNSPEC_TEOF) {
    if (tok != VULNSPEC_TLPAREN) {
      status = VULNSPEC_EUNEXPECTED_TOKEN;
      goto done;
    }

    nodet = read_symbol(p);
    switch(nodet) {
    case VULNSPEC_CVE_NODE:
      curr = cve(p);
      break;
    default:
      status = VULNSPEC_EUNEXPECTED_TOKEN;
      goto done;
    }

    expect(p, VULNSPEC_TRPAREN);

    /* if we have a previous node, link it to the current one */
    if (prev.offset != 0) {
      struct vulnspec_cve_node *node = NOD(p, prev);
      assert(node->type == VULNSPEC_CVE_NODE);
      node->next = curr;
    }

    prev = curr;
  }

  status = 0;
done:
  vulnspec_reader_cleanup(&p->r);
  return status;
}
