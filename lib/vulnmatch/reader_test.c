#include <string.h>

#include "lib/util/test.h"
#include "lib/util/macros.h"
#include "lib/vulnmatch/vulnmatch.h"

#define MAX_TOKENS 10

static int test_read_token() {
  long longval;
  double dblval;
  const char *sval;
  int status = TEST_OK;
  struct vulnmatch_reader r;
  size_t i;
  FILE *fp;
  size_t tok;
  enum vulnmatch_token tokval;
  static struct {
    char *input;
    size_t ntokens;
    enum vulnmatch_token tokens[MAX_TOKENS];
    long longvals[MAX_TOKENS];
    double dblvals[MAX_TOKENS];
    const char *svals[MAX_TOKENS];
  } tests[] = {
    {
      .input    = " ",
      .ntokens  = 1,
      .tokens   = {VULNMATCH_TEOF},
    },
    {
      .input    = "wut",
      .ntokens  = 1,
      .tokens   = {VULNMATCH_TINVALID},
    },
    {
      .input    = "\"wut wut\"",
      .ntokens  = 2,
      .tokens   = {VULNMATCH_TSTRING, VULNMATCH_TEOF},
      .svals    = {"wut wut"},
    },
    {
      .input    = "\"wut \\\" wut\"",
      .ntokens  = 2,
      .tokens   = {VULNMATCH_TSTRING, VULNMATCH_TEOF},
      .svals    = {"wut \" wut"},
    },
    {
      .input    = "\"wut \\\\ wut\"",
      .ntokens  = 2,
      .tokens   = {VULNMATCH_TSTRING, VULNMATCH_TEOF},
      .svals    = {"wut \\ wut"},
    },
    {
      .input    = "42",
      .ntokens  = 2,
      .tokens   = {VULNMATCH_TLONG, VULNMATCH_TEOF},
      .longvals = {42, 0},
    },
    {
      .input    = "-1",
      .ntokens  = 2,
      .tokens   = {VULNMATCH_TLONG, VULNMATCH_TEOF},
      .longvals = {-1, 0},
    },
    {
      .input   = "42.2",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TDOUBLE, VULNMATCH_TEOF},
      .dblvals = {42.2, .0},
    },
    {
      .input   = ".2",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TDOUBLE, VULNMATCH_TEOF},
      .dblvals = {.2, .0},
    },
    {
      .input   = "-.2",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TDOUBLE, VULNMATCH_TEOF},
      .dblvals = {-.2, .0},
    },
    {
      .input   = "(",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TLPAREN, VULNMATCH_TEOF},
    },
    {
      .input   = ")",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TRPAREN, VULNMATCH_TEOF},
    },
    {
      .input   = "^",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TAND, VULNMATCH_TEOF},
    },
    {
      .input   = "v",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TOR, VULNMATCH_TEOF},
    },
    {
      .input   = "<",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TLT, VULNMATCH_TEOF},
    },
    {
      .input   = "<=",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TLE, VULNMATCH_TEOF},
    },
    {
      .input   = "=",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TEQ, VULNMATCH_TEOF},
    },
    {
      .input   = ">=",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TGE, VULNMATCH_TEOF},
    },
    {
      .input   = ">",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TGT, VULNMATCH_TEOF},
    },
    {
      .input   = "cve",
      .ntokens = 2,
      .tokens  = {VULNMATCH_TCVE, VULNMATCH_TEOF},
    },
    {
      .input    = "(cve 2.2 3 \"foo \\\"bar\\\"\" -222.11)",
      .ntokens  = 8,
      .tokens   = {VULNMATCH_TLPAREN, VULNMATCH_TCVE, VULNMATCH_TDOUBLE,
                   VULNMATCH_TLONG, VULNMATCH_TSTRING, VULNMATCH_TDOUBLE,
                   VULNMATCH_TRPAREN, VULNMATCH_TEOF},
      .dblvals  = {.0, .0, 2.2, .0, .0, -222.11, .0, .0},
      .longvals = {0, 0, 0, 3, 0, 0, 0, 0},
      .svals    = {"", "", "", "", "foo \"bar\"", "", "", ""},
    },
  };

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    fp = fmemopen(tests[i].input, strlen(tests[i].input), "rb");
    if (!fp) {
      perror("fmemopen");
      status = TEST_FAIL;
      continue;
    }

    vulnmatch_reader_init(&r, fp);
    for (tok = 0; tok < tests[i].ntokens; tok++) {
      tokval = vulnmatch_read_token(&r);
      if (tokval != tests[i].tokens[tok]) {
        TEST_LOGF("index:%zu token:%zu expected:%s was:%s\n", i, tok,
            vulnmatch_token2str(tests[i].tokens[tok]),
            vulnmatch_token2str(tokval));
        status = TEST_FAIL;
        break;
      }

      if (tokval == VULNMATCH_TLONG) {
        longval = vulnmatch_reader_long(&r);
        if (tests[i].longvals[tok] != longval) {
          TEST_LOGF("index:%zu token:%zu expected:%ld was:%ld\n", i, tok,
              tests[i].longvals[tok], longval);
          status = TEST_FAIL;
          break;
        }
      } else if (tokval == VULNMATCH_TDOUBLE) {
        dblval = vulnmatch_reader_double(&r);
        if (tests[i].dblvals[tok] != dblval) {
          TEST_LOGF("index:%zu token:%zu expected:%f was:%f\n", i, tok,
              tests[i].dblvals[tok], dblval);
          status = TEST_FAIL;
          break;
        }
      } else if (tokval == VULNMATCH_TSTRING) {
        sval = vulnmatch_reader_string(&r);
        if (strcmp(tests[i].svals[tok], sval) != 0) {
          TEST_LOGF("index:%zu token:%zu expected:%s was:%s\n", i, tok,
              tests[i].svals[tok], sval);
          status = TEST_FAIL;
          break;
        }
      }
    }

    vulnmatch_reader_cleanup(&r);
    fclose(fp);
  }

  return status;
}

TEST_ENTRY(
  {"read_token", test_read_token},
);
