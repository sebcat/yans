#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "lib/util/macros.h"
#include "lib/vulnmatch/vulnmatch.h"

/* dereference a struct vulnmatch_value */
#define NOD(p, v) ((void*)((p)->progn.buf.data + (v).offset))

/* initial size of string table */
#define STRTAB_ENTRIES 4096

struct strtab_entry {
  char *str;
  size_t len;
  struct vulnmatch_value val;
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
    struct vulnmatch_value val) {
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

static void progn_alloc(struct vulnmatch_parser *p, size_t len,
    struct vulnmatch_value *out) {
  int ret;

  ret = vulnmatch_progn_alloc(&p->progn, len, out);
  if (ret != 0) {
    longjmp(p->errjmp, VULNMATCH_EMALLOC);
  }
}

static inline void bail(struct vulnmatch_parser *p, int err) {
  longjmp(p->errjmp, err);
}

static inline void expect(struct vulnmatch_parser *p, enum vulnmatch_token t) {
  if (vulnmatch_read_token(&p->r) != t) {
    bail(p, VULNMATCH_EUNEXPECTED_TOKEN);
  }
}

static void loads(struct vulnmatch_parser *p,
    struct vulnmatch_cvalue *out) {
  struct vulnmatch_value val;
  int ret;
  void *tblentry;
  struct strtab_entry e;

  /* load the string from the reader */
  expect(p, VULNMATCH_TSTRING);
  e.str = (char*)vulnmatch_reader_string(&p->r, &e.len);
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
      bail(p, VULNMATCH_ESTRTAB);
    }

    ret = objtbl_insert(&p->strtab, ent);
    if (ret != OBJTBL_OK) {
      strtab_free(ent);
      bail(p, VULNMATCH_ESTRTAB);
    }

    out->length = ent->len;
    out->value = ent->val;
  } else {
    bail(p, VULNMATCH_ESTRTAB);
  }
}

static enum vulnmatch_node_type token2node(enum vulnmatch_token t) {
  switch(t) {
  case VULNMATCH_TOR:
    return VULNMATCH_OR_NODE;
  case VULNMATCH_TAND:
    return VULNMATCH_AND_NODE;
  case VULNMATCH_TLT:
    return VULNMATCH_LT_NODE;
  case VULNMATCH_TLE:
    return VULNMATCH_LE_NODE;
  case VULNMATCH_TEQ:
    return VULNMATCH_EQ_NODE;
  case VULNMATCH_TGE:
    return VULNMATCH_GE_NODE;
  case VULNMATCH_TGT:
    return VULNMATCH_GT_NODE;
  case VULNMATCH_TCVE:
    return VULNMATCH_CVE_NODE;
  default:
    return VULNMATCH_INVALID_NODE;
  }
}

static struct vulnmatch_value compar(struct vulnmatch_parser *p,
    enum vulnmatch_node_type t) {
  struct vulnmatch_value compar;
  struct vulnmatch_cvalue cval;
  const char *versionstr;

  progn_alloc(p, sizeof(struct vulnmatch_compar_node), &compar);

  loads(p, &cval); /* load vend/prod string */
  expect(p, VULNMATCH_TSTRING); /* version */
  versionstr = (char*)vulnmatch_reader_string(&p->r, NULL);
  do {
    struct vulnmatch_compar_node *node = NOD(p, compar);
    node->vendprod = cval;
    vaguever_init(&node->version, versionstr);
    node->type = t;
  } while(0);

  expect(p, VULNMATCH_TRPAREN);
  return compar;
}

static struct vulnmatch_value vulnexpr(struct vulnmatch_parser *p);

static struct vulnmatch_value boolean(struct vulnmatch_parser *p,
    enum vulnmatch_node_type nodet) {
  struct vulnmatch_value bnode = {0};
  struct vulnmatch_value curr;
  struct vulnmatch_value prev;
  struct vulnmatch_value val;
  enum vulnmatch_token tok;

  prev.offset = 0;
  curr = bnode;
  while ((tok = vulnmatch_read_token(&p->r)) == VULNMATCH_TLPAREN) {
    progn_alloc(p, sizeof(struct vulnmatch_boolean_node), &curr);
    if (bnode.offset == 0) {
      bnode = curr; /* save first entry */
    }

    val = vulnexpr(p);
    do {
      struct vulnmatch_boolean_node *node = NOD(p, curr);
      node->type = nodet;
      node->value = val;
    } while(0);

    if (prev.offset != 0) {
      do {
        struct vulnmatch_boolean_node *node = NOD(p, prev);
        node->next = curr;
      } while(0);

    }

    prev = curr;
  }

  if (tok != VULNMATCH_TRPAREN) {
    bail(p, VULNMATCH_EUNEXPECTED_TOKEN);
  }

  if (bnode.offset == 0) {
    bail(p, VULNMATCH_EUNEXPECTED_TOKEN);
  }

  return bnode;
}

static struct vulnmatch_value vulnexpr(struct vulnmatch_parser *p) {
  struct vulnmatch_value val = {0};
  enum vulnmatch_token t;
  enum vulnmatch_node_type nodet;

  t = vulnmatch_read_token(&p->r);
  switch (t) {
  case VULNMATCH_TLT:
  case VULNMATCH_TLE:
  case VULNMATCH_TEQ:
  case VULNMATCH_TGE:
  case VULNMATCH_TGT:
    nodet = token2node(t);
    val = compar(p, nodet);
    break;
  case VULNMATCH_TAND:
  case VULNMATCH_TOR:
    nodet = token2node(t);
    val = boolean(p, nodet);
    break;
  default:
    bail(p, VULNMATCH_EUNEXPECTED_TOKEN);
    break;
  }

  return val;
}

static struct vulnmatch_value cve(struct vulnmatch_parser *p) {
  struct vulnmatch_value cve;
  struct vulnmatch_value vexpr;
  struct vulnmatch_cvalue cval;

  progn_alloc(p, sizeof(struct vulnmatch_cve_node), &cve);
  loads(p, &cval);
  do {
    struct vulnmatch_cve_node *node = NOD(p, cve);
    node->id = cval;
    expect(p, VULNMATCH_TDOUBLE);
    node->cvss3_base = (uint32_t)(vulnmatch_reader_double(&p->r) * 100.0);
    node->type = VULNMATCH_CVE_NODE;
  } while(0);

  loads(p, &cval);
  do {
    struct vulnmatch_cve_node *node = NOD(p, cve);
    node->description = cval;
  } while(0);

  expect(p, VULNMATCH_TLPAREN);
  vexpr = vulnexpr(p);
  do {
    struct vulnmatch_cve_node *node = NOD(p, cve);
    node->vulnexpr = vexpr;
  } while(0);

  return cve;
}

int vulnmatch_parser_init(struct vulnmatch_parser *p) {
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

  return vulnmatch_progn_init(&p->progn);
}

static int free_strtab(void *data, void *elem) {
  strtab_free(elem);
  return 1;
}

void vulnmatch_parser_cleanup(struct vulnmatch_parser *p) {
  objtbl_foreach(&p->strtab, free_strtab, NULL);
  objtbl_cleanup(&p->strtab);
  vulnmatch_progn_cleanup(&p->progn);
}

int vulnmatch_parse(struct vulnmatch_parser *p, FILE *in) {
  enum vulnmatch_token tok;
  struct vulnmatch_value curr;
  struct vulnmatch_value prev;
  int status;

  status = vulnmatch_reader_init(&p->r, in);
  if (status != 0) {
    return VULNMATCH_EMALLOC;
  }

  buf_clear(&p->progn.buf);
  buf_adata(&p->progn.buf, VULNMATCH_HEADER, VULNMATCH_HEADER_SIZE);
  if ((status = setjmp(p->errjmp)) != 0) {
    goto done;
  }

  prev.offset = 0;
  while ((tok = vulnmatch_read_token(&p->r)) != VULNMATCH_TEOF) {
    if (tok != VULNMATCH_TLPAREN) {
      status = VULNMATCH_EUNEXPECTED_TOKEN;
      goto done;
    }

    tok = vulnmatch_read_token(&p->r);
    switch(tok) {
    case VULNMATCH_TCVE:
      curr = cve(p);
      break;
    default:
      status = VULNMATCH_EUNEXPECTED_TOKEN;
      goto done;
    }

    expect(p, VULNMATCH_TRPAREN);

    /* if we have a previous node, link it to the current one */
    if (prev.offset != 0) {
      struct vulnmatch_cve_node *node = NOD(p, prev);
      assert(node->type == VULNMATCH_CVE_NODE);
      node->next = curr;
    }

    prev = curr;
  }

  status = 0;
done:
  vulnmatch_reader_cleanup(&p->r);
  return status;
}
