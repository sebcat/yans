#ifndef YANS_VULNSPEC_H__
#define YANS_VULNSPEC_H__

#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>

#include "lib/util/buf.h"
#include "lib/util/objtbl.h"
#include "lib/util/vaguever.h"

#define VULNSPEC_OK                 0
#define VULNSPEC_EUNEXPECTED_TOKEN -1
#define VULNSPEC_EMALLOC           -2
#define VULNSPEC_ESTRTAB           -3
#define VULNSPEC_EINVALID_OFFSET   -4
#define VULNSPEC_EINVALID_NODE     -5
#define VULNSPEC_ELOAD             -6
#define VULNSPEC_EHEADER           -7

#define VULNSPEC_HEADER                     "VM0\0\0\0\0"
#define VULNSPEC_HEADER_SIZE     sizeof(VULNSPEC_HEADER)

enum vulnspec_token {
  VULNSPEC_TINVALID,
  VULNSPEC_TEOF,
  VULNSPEC_TLPAREN,
  VULNSPEC_TRPAREN,
  VULNSPEC_TSTRING,
  VULNSPEC_TLONG,
  VULNSPEC_TDOUBLE,
  VULNSPEC_TSYMBOL,
};

enum vulnspec_node_type {
  VULNSPEC_INVALID_NODE,
  VULNSPEC_CVE_NODE,
  VULNSPEC_OR_NODE,
  VULNSPEC_AND_NODE,
  VULNSPEC_SEQ_NODE,
  VULNSPEC_LT_NODE,
  VULNSPEC_LE_NODE,
  VULNSPEC_EQ_NODE,
  VULNSPEC_GE_NODE,
  VULNSPEC_GT_NODE,
};

struct vulnspec_value {
  uint32_t offset;
};

/* for variable length allocations where the size is not determined by
 * the node type */
struct vulnspec_cvalue {
  uint32_t length;
  struct vulnspec_value value;
};

struct vulnspec_compar_node {
  enum vulnspec_node_type type;
  struct vulnspec_cvalue vendprod;
  struct vaguever_version version;
};

struct vulnspec_boolean_node {
  enum vulnspec_node_type type;
  struct vulnspec_value next;
  struct vulnspec_value value;
};

struct vulnspec_cve_node {
  enum vulnspec_node_type type;
  struct vulnspec_value next;
  uint32_t cvss2_base; /* represented as fixed point */
  uint32_t cvss3_base; /* represented as fixed point */
  struct vulnspec_cvalue id;
  struct vulnspec_cvalue description;
  struct vulnspec_value vulnexpr;
};

struct vulnspec_progn {
  buf_t buf;
};

struct vulnspec_reader {
  char symbol[32];
  FILE *input;
  size_t row;
  size_t col;
  size_t lastcol;
  size_t depth;
  buf_t sval;
  union {
    double dval;
    long ival;
  } num;
};

struct vulnspec_parser {
  struct objtbl_ctx strtab;
  struct vulnspec_progn progn;
  struct vulnspec_reader r;
  jmp_buf errjmp;
};

struct vulnspec_match {
  const char *id;
  float cvss2_base;
  float cvss3_base;
  const char *desc;
};

struct vulnspec_interp {
  const char *data;
  size_t len;
  int (*on_match)(struct vulnspec_match *, void *);
  void *on_match_data;
  const char *curr_vendprod;
  struct vaguever_version curr_version;
  jmp_buf errjmp;
};

static inline long vulnspec_reader_long(struct vulnspec_reader *r) {
  return r->num.ival;
}

static inline double vulnspec_reader_double(struct vulnspec_reader *r) {
  return r->num.dval;
}

static inline const char *vulnspec_reader_symbol(
    struct vulnspec_reader *r) {
  return r->symbol;
}

static inline const char *vulnspec_reader_string(
    struct vulnspec_reader *r, size_t *len) {
  if (r->sval.len > 0) {
    if (len) *len = r->sval.len - 1;
    return r->sval.data;
  } else {
    if (len) *len = 0;
    return "";
  }
}

int vulnspec_reader_init(struct vulnspec_reader *r, FILE *input);
void vulnspec_reader_cleanup(struct vulnspec_reader *r);
enum vulnspec_token vulnspec_read_token(struct vulnspec_reader *r);
const char *vulnspec_token2str(enum vulnspec_token t);

int vulnspec_progn_init(struct vulnspec_progn *progn);
void vulnspec_progn_cleanup(struct vulnspec_progn *progn);
int vulnspec_progn_alloc(struct vulnspec_progn *progn,
    size_t len, struct vulnspec_value *out);
static inline void *vulnspec_progn_deref_unsafe(
    struct vulnspec_progn *progn, struct vulnspec_value *val) {
  return progn->buf.data + val->offset;
}
void *vulnspec_progn_deref(struct vulnspec_progn *progn,
    struct vulnspec_value *val, size_t len);
static inline void *vulnspec_progn_cderef(struct vulnspec_progn *progn,
    struct vulnspec_cvalue *val) {
  return vulnspec_progn_deref(progn, &val->value, val->length);
}

int vulnspec_parser_init(struct vulnspec_parser *p);
void vulnspec_parser_cleanup(struct vulnspec_parser *p);
int vulnspec_parse(struct vulnspec_parser *p, FILE *in);
static inline const char *vulnspec_parser_data(struct vulnspec_parser *p,
  size_t *len) {
  if (len) *len = p->progn.buf.len;
  return p->progn.buf.data;
}

void vulnspec_init(struct vulnspec_interp *interp,
    int (*on_match)(struct vulnspec_match *, void *));
void vulnspec_unloadfile(struct vulnspec_interp *interp);
int vulnspec_load(struct vulnspec_interp *interp, const char *data,
  size_t len);
int vulnspec_loadfile(struct vulnspec_interp *interp, int fd);
int vulnspec_eval(struct vulnspec_interp *interp, const char *vendprod,
    const char *version, void *data);
#endif
