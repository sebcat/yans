#ifndef YANS_VULNMATCH_H__
#define YANS_VULNMATCH_H__

#include <setjmp.h>
#include <stdio.h>
#include "lib/util/buf.h"

#define VULNMATCH_EUNEXPECTED_TOKEN -1
#define VULNMATCH_EMALLOC           -2

// (cve "CVE-2019-0001" 8.2 "long description string"
//   (or
//     (< "vendor/product" "version")
//     (> "vendor/product" "version")))

enum vulnmatch_token {
  VULNMATCH_TINVALID,
  VULNMATCH_TEOF,
  VULNMATCH_TLPAREN,
  VULNMATCH_TRPAREN,
  VULNMATCH_TSTRING,
  VULNMATCH_TLONG,
  VULNMATCH_TDOUBLE,
  VULNMATCH_TOR,
  VULNMATCH_TAND,
  VULNMATCH_TLT,
  VULNMATCH_TLE,
  VULNMATCH_TEQ,
  VULNMATCH_TGE,
  VULNMATCH_TGT,
  VULNMATCH_TCVE,
};

enum vulnmatch_node_type {
  VULNMATCH_INVALID_NODE,
  VULNMATCH_CVE_NODE,
  VULNMATCH_OR_NODE,
  VULNMATCH_AND_NODE,
  VULNMATCH_SEQ_NODE,
  VULNMATCH_LT_NODE,
  VULNMATCH_LE_NODE,
  VULNMATCH_EQ_NODE,
  VULNMATCH_GE_NODE,
  VULNMATCH_GT_NODE,
};

struct vulnmatch_value {
  size_t offset;
};

/* for variable length allocations where the size is not determined by
 * the node type */
struct vulnmatch_cvalue {
  size_t length;
  struct vulnmatch_value value;
};

struct vulnmatch_progn {
  buf_t buf;
};

struct vulnmatch_reader {
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


struct vulnmatch_compar_node {
  enum vulnmatch_node_type type;
  struct vulnmatch_cvalue vendprod;
  struct vulnmatch_cvalue version;
};

struct vulnmatch_boolean_node {
  enum vulnmatch_node_type type;
  struct vulnmatch_value next;
  struct vulnmatch_value value;
};

struct vulnmatch_cve_node {
  enum vulnmatch_node_type type;
  struct vulnmatch_value next;
  struct vulnmatch_cvalue id;
  double cvss3_base;
  struct vulnmatch_cvalue description;
  struct vulnmatch_value vulnexpr;
};

struct vulnmatch_parser {
  struct vulnmatch_progn progn;
  struct vulnmatch_reader r;
  jmp_buf errjmp;
};


static inline long vulnmatch_reader_long(struct vulnmatch_reader *r) {
  return r->num.ival;
}

static inline double vulnmatch_reader_double(struct vulnmatch_reader *r) {
  return r->num.dval;
}

static inline const char *vulnmatch_reader_string(
    struct vulnmatch_reader *r, size_t *len) {
  if (r->sval.len > 0) {
    if (len) *len = r->sval.len - 1;
    return r->sval.data;
  } else {
    if (len) *len = 0;
    return "";
  }
}

int vulnmatch_reader_init(struct vulnmatch_reader *r, FILE *input);
void vulnmatch_reader_cleanup(struct vulnmatch_reader *r);
enum vulnmatch_token vulnmatch_read_token(struct vulnmatch_reader *r);
const char *vulnmatch_token2str(enum vulnmatch_token t);

int vulnmatch_progn_init(struct vulnmatch_progn *progn);
void vulnmatch_progn_cleanup(struct vulnmatch_progn *progn);
int vulnmatch_progn_alloc(struct vulnmatch_progn *progn,
    size_t len, struct vulnmatch_value *out);
static inline void *vulnmatch_progn_deref_unsafe(
    struct vulnmatch_progn *progn, struct vulnmatch_value *val) {
  return progn->buf.data + val->offset;
}
void *vulnmatch_progn_deref(struct vulnmatch_progn *progn,
    struct vulnmatch_value *val, size_t len);
static inline void *vulnmatch_progn_cderef(struct vulnmatch_progn *progn,
    struct vulnmatch_cvalue *val) {
  return vulnmatch_progn_deref(progn, &val->value, val->length);
}

int vulnmatch_parser_init(struct vulnmatch_parser *p);
void vulnmatch_parser_cleanup(struct vulnmatch_parser *p);
int vulnmatch_parse(struct vulnmatch_parser *p, FILE *in);
#endif
