#include <string.h>
#include <assert.h>

#include "lib/vulnmatch/vulnmatch.h"

/* dereference a struct vulnmatch_value */
#define NOD(p, v) ((void*)((p)->progn.buf.data + (v).offset))

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
  const char *s;
  size_t len;

  expect(p, VULNMATCH_TSTRING); /* load a string from input to parser */
  s = vulnmatch_reader_string(&p->r, &len); /* fetch the string */
  len++; /* include null byte */
  progn_alloc(p, len, &val);
  memcpy(NOD(p, val), s, len);
  out->length = len;
  out->value = val;
}

static enum vulnmatch_node_type token2node( enum vulnmatch_token t) {
  switch(t) {
  case VULNMATCH_TOR:
    return VULNMATCH_OR_NODE;
  case VULNMATCH_TAND:
    return VULNMATCH_AND_NODE;
  case VULNMATCH_TLT:
    return VULNMATCH_LT_NODE;
  case VULNMATCH_TLE:
    return VULNMATCH_GT_NODE;
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

  progn_alloc(p, sizeof(struct vulnmatch_compar_node), &compar);

  loads(p, &cval);
  do {
    struct vulnmatch_compar_node *node = NOD(p, compar);
    node->vendprod = cval;
    node->type = t;
  } while(0);

  loads(p, &cval);
  do {
    struct vulnmatch_compar_node *node = NOD(p, compar);
    node->version = cval;
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
    node->cvss3_base = vulnmatch_reader_double(&p->r);
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
  memset(p, 0, sizeof(*p));
  return vulnmatch_progn_init(&p->progn);
}

void vulnmatch_parser_cleanup(struct vulnmatch_parser *p) {
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
  buf_adata(&p->progn.buf, "VM0\0", 4);
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
