#ifndef YANS_VULNMATCH_H__
#define YANS_VULNMATCH_H__

#include "lib/util/buf.h"

// (cve "CVE-2019-0001" 8.2 "long description string"
//   (or
//     (< "cpe:...")
//     (> "cpe:...")))

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
  VULNMATCH_LAST /* Not an actual token, must be last. */
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

static inline long vulnmatch_reader_long(struct vulnmatch_reader *r) {
  return r->num.ival;
}

static inline double vulnmatch_reader_double(struct vulnmatch_reader *r) {
  return r->num.dval;
}

static inline const char *vulnmatch_reader_string(
    struct vulnmatch_reader *r) {
  if (r->sval.len > 0) {
    return r->sval.data;
  } else {
    return "";
  }
}

int vulnmatch_reader_init(struct vulnmatch_reader *r, FILE *input);
void vulnmatch_reader_cleanup(struct vulnmatch_reader *r);
enum vulnmatch_token vulnmatch_read_token(struct vulnmatch_reader *r);
const char *vulnmatch_token2str(enum vulnmatch_token t);

#endif
