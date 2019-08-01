#include <string.h>

#include "lib/vulnmatch/vulnmatch.h"

#define PROGN_DEFAULT_SIZE (1024 * 512)

static void progn_alloc(struct vulnmatch_parser *p, size_t len,
    struct vulnmatch_value *out) {
  int ret;

  ret = vulnmatch_progn_alloc(&p->progn, len, out);
  if (ret != 0) {
    longjmp(p->errjmp, VULNMATCH_EMALLOC);
  }
}

static inline void expect(struct vulnmatch_parser *p, enum vulnmatch_token t) {
  if (vulnmatch_read_token(&p->r) != t) {
    longjmp(p->errjmp, VULNMATCH_EUNEXPECTED_TOKEN);
  }
}

#define NOD(p, v) ((void*)((p)->progn.buf.data + (v).offset))

static struct vulnmatch_value cve(struct vulnmatch_parser *p) {
  struct vulnmatch_value cve;
  struct vulnmatch_value val;
  const char *s;
  size_t len;

  progn_alloc(p, sizeof(struct vulnmatch_cve_node), &cve);

  expect(p, VULNMATCH_TSTRING);
  s = vulnmatch_reader_string(&p->r, &len);
  progn_alloc(p, len + 1, &val);
  memcpy(NOD(p, val), s, len+1);
  do {
    struct vulnmatch_cve_node *tmp;
    tmp = NOD(p, cve);
    tmp->id.length = len + 1;
    tmp->id.value = val;

    expect(p, VULNMATCH_TDOUBLE);
    tmp->cvss3_base = vulnmatch_reader_double(&p->r);
  } while(0);

  expect(p, VULNMATCH_TSTRING);
  s = vulnmatch_reader_string(&p->r, &len);
  progn_alloc(p, len + 1, &val);
  memcpy(NOD(p, val), s, len + 1);
  do {
    struct vulnmatch_cve_node *tmp;
    tmp = NOD(p, cve);
    tmp->description.length = len + 1;
    tmp->description.value = val;
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
  struct vulnmatch_value val;
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

    /* allocate a new node and link the prev node (if any) to this one */
    progn_alloc(p, sizeof(struct vulnmatch_node), &curr);
    if (prev.offset != 0) {
      struct vulnmatch_node *tmp;
      tmp = NOD(p, prev);
      tmp->next = curr;
    }

    /* update the reference to the prev node to the now current one */
    prev = curr;

    tok = vulnmatch_read_token(&p->r);
    switch(tok) {
    case VULNMATCH_TCVE:
      val = cve(p);
      break;
    default:
      status = VULNMATCH_EUNEXPECTED_TOKEN;
      goto done;
    }

    expect(p, VULNMATCH_TRPAREN);

    /* assign node fields */
    do {
      struct vulnmatch_node *tmp;
      tmp = NOD(p, curr);
      tmp->type = tok;
      tmp->value = val;
    } while(0);
  }

  status = 0;
done:
  vulnmatch_reader_cleanup(&p->r);
  return status;
}
