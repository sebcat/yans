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

#include "lib/util/hexdump.h"
#include "lib/util/test.h"
#include "lib/util/macros.h"
#include "lib/vulnspec/vulnspec.h"

#define MAX_TOKENS 10

static int test_read_token() {
  long longval;
  double dblval;
  const char *sval;
  int status = TEST_OK;
  struct vulnspec_reader r;
  size_t i;
  FILE *fp;
  size_t tok;
  enum vulnspec_token tokval;
  static struct {
    char *input;
    size_t ntokens;
    enum vulnspec_token tokens[MAX_TOKENS];
    long longvals[MAX_TOKENS];
    double dblvals[MAX_TOKENS];
    const char *svals[MAX_TOKENS];
    const char *symvals[MAX_TOKENS];
  } tests[] = {
    {
      .input    = " ",
      .ntokens  = 1,
      .tokens   = {VULNSPEC_TEOF},
    },
    {
      .input    = "wut",
      .ntokens  = 2,
      .tokens   = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {"wut"},
    },
    {
      .input    = "\"wut wut\"",
      .ntokens  = 2,
      .tokens   = {VULNSPEC_TSTRING, VULNSPEC_TEOF},
      .svals    = {"wut wut"},
    },
    {
      .input    = "\"wut \\\" wut\"",
      .ntokens  = 2,
      .tokens   = {VULNSPEC_TSTRING, VULNSPEC_TEOF},
      .svals    = {"wut \" wut"},
    },
    {
      .input    = "\"wut \\\\ wut\"",
      .ntokens  = 2,
      .tokens   = {VULNSPEC_TSTRING, VULNSPEC_TEOF},
      .svals    = {"wut \\ wut"},
    },
    {
      .input    = "42",
      .ntokens  = 2,
      .tokens   = {VULNSPEC_TLONG, VULNSPEC_TEOF},
      .longvals = {42, 0},
    },
    {
      .input    = "-1",
      .ntokens  = 2,
      .tokens   = {VULNSPEC_TLONG, VULNSPEC_TEOF},
      .longvals = {-1, 0},
    },
    {
      .input   = "42.2",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TDOUBLE, VULNSPEC_TEOF},
      .dblvals = {42.2, .0},
    },
    {
      .input   = ".2",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TDOUBLE, VULNSPEC_TEOF},
      .dblvals = {.2, .0},
    },
    {
      .input   = "-.2",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TDOUBLE, VULNSPEC_TEOF},
      .dblvals = {-.2, .0},
    },
    {
      .input   = "(",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TLPAREN, VULNSPEC_TEOF},
    },
    {
      .input   = ")",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TRPAREN, VULNSPEC_TEOF},
    },
    {
      .input   = "^",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {"^"},
    },
    {
      .input   = "v",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {"v"},
    },
    {
      .input   = "<",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {"<"},
    },
    {
      .input   = "<=",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {"<="},
    },
    {
      .input   = "=",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {"="},
    },
    {
      .input   = ">=",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {">="},
    },
    {
      .input   = ">",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {">"},
    },
    {
      .input   = "cve",
      .ntokens = 2,
      .tokens  = {VULNSPEC_TSYMBOL, VULNSPEC_TEOF},
      .symvals  = {"cve"},
    },
    {
      .input    = "(cve 2.2 3 \"foo \\\"bar\\\"\" -222.11)",
      .ntokens  = 8,
      .tokens   = {VULNSPEC_TLPAREN, VULNSPEC_TSYMBOL, VULNSPEC_TDOUBLE,
                   VULNSPEC_TLONG, VULNSPEC_TSTRING, VULNSPEC_TDOUBLE,
                   VULNSPEC_TRPAREN, VULNSPEC_TEOF},
      .dblvals  = {.0, .0, 2.2, .0, .0, -222.11, .0, .0},
      .longvals = {0, 0, 0, 3, 0, 0, 0, 0},
      .svals    = {"", "", "", "", "foo \"bar\"", "", "", ""},
      .symvals  = {NULL, "cve", NULL, NULL, NULL, NULL, NULL, NULL},
    },
  };

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    fp = fmemopen(tests[i].input, strlen(tests[i].input), "rb");
    if (!fp) {
      perror("fmemopen");
      status = TEST_FAIL;
      continue;
    }

    vulnspec_reader_init(&r, fp);
    for (tok = 0; tok < tests[i].ntokens; tok++) {
      tokval = vulnspec_read_token(&r);
      if (tokval != tests[i].tokens[tok]) {
        TEST_LOGF("index:%zu token:%zu expected:%s was:%s\n", i, tok,
            vulnspec_token2str(tests[i].tokens[tok]),
            vulnspec_token2str(tokval));
        status = TEST_FAIL;
        break;
      }

      if (tokval == VULNSPEC_TLONG) {
        longval = vulnspec_reader_long(&r);
        if (tests[i].longvals[tok] != longval) {
          TEST_LOGF("index:%zu token:%zu expected:%ld was:%ld\n", i, tok,
              tests[i].longvals[tok], longval);
          status = TEST_FAIL;
          break;
        }
      } else if (tokval == VULNSPEC_TDOUBLE) {
        dblval = vulnspec_reader_double(&r);
        if (tests[i].dblvals[tok] != dblval) {
          TEST_LOGF("index:%zu token:%zu expected:%f was:%f\n", i, tok,
              tests[i].dblvals[tok], dblval);
          status = TEST_FAIL;
          break;
        }
      } else if (tokval == VULNSPEC_TSTRING) {
        sval = vulnspec_reader_string(&r, NULL);
        if (strcmp(tests[i].svals[tok], sval) != 0) {
          TEST_LOGF("index:%zu token:%zu expected:%s was:%s\n", i, tok,
              tests[i].svals[tok], sval);
          status = TEST_FAIL;
          break;
        }
      } else if (tokval == VULNSPEC_TSYMBOL) {
        sval = vulnspec_reader_symbol(&r);
        if (strcmp(tests[i].symvals[tok], sval) != 0) {
          TEST_LOGF("index:%zu token:%zu expected:%s was:%s\n", i, tok,
              tests[i].symvals[tok], sval);
          status = TEST_FAIL;
          break;
        }
      }
    }

    vulnspec_reader_cleanup(&r);
    fclose(fp);
  }

  return status;
}

static int test_parse_ok() {
  static struct {
    char *input;
  } tests[] = {
    {"(cve \"CVE-2019-02-17\" 8.8 1.0 \"my desc\"\n"
     "  (= \"foo/bar\" \"1.2.3\"))"},
    {"(cve \"CVE-2019-02-17\" 8.8 1.0 \"my desc\"\n"
     "  (= \"foo/bar\" \"1.2.3\"))\n"
     "(cve \"CVE-2019-02-18\" 8.8 1.0 \"my desc\"\n"
     "  (= \"foo/bar\" \"1.2.3\"))\n"},
    {"(cve \"CVE-2019-02-17\" 8.8 1.0 \"my desc\"\n"
     "  (v\n"
     "    (^\n"
     "      (>= \"foo/bar\" \"1.2.3\")\n"
     "      (<= \"foo/bar\" \"2.0.0\"))\n"
     "    (^\n"
     "      (>= \"bar/baz\" \"6.6.6\")\n"
     "      (<= \"bar/baz\" \"7.7.7\"))))\n"},
    {"(cve \"CVE-2019-02-17\" 8.8 1.0 \"my desc\"\n"
     "  (v\n"
     "    (^\n"
     "      (>= \"foo/bar\" \"1.2.3\")\n"
     "      (<= \"foo/bar\" \"2.0.0\"))\n"
     "    (^\n"
     "      (>= \"bar/baz\" \"6.6.6\")\n"
     "      (<= \"bar/baz\" \"7.7.7\"))))\n"
     "(cve \"CVE-2019-02-17\" 8.8 1.0 \"my desc\"\n"
     "  (v\n"
     "    (^\n"
     "      (>= \"foo/bar\" \"1.2.3\")\n"
     "      (<= \"foo/bar\" \"2.0.0\"))\n"
     "    (^\n"
     "      (>= \"bar/baz\" \"6.6.6\")\n"
     "      (<= \"bar/baz\" \"7.7.7\"))))\n"}
  };
  int status = TEST_OK;
  size_t i;
  FILE *fp;
  struct vulnspec_parser p;
  int ret;
  char *envdbg;
  int debug = 0;
  struct vulnspec_interp interp;

  envdbg = getenv("TEST_DEBUG");
  if (envdbg != NULL && *envdbg == '1') {
    debug = 1;
  }

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    ret = vulnspec_parser_init(&p);
    if (ret != 0) {
      TEST_LOGF("index:%zu vulnspec_parser_init failure", i);
      status = TEST_FAIL;
      continue;
    }

    fp = fmemopen(tests[i].input, strlen(tests[i].input), "rb");
    if (!fp) {
      TEST_LOGF("index:%zu fmemopen failure", i);
      status = TEST_FAIL;
      goto parser_cleanup;
    }

    ret = vulnspec_parse(&p, fp);
    if (ret != 0) {
      TEST_LOGF("index:%zu vulnspec_parse %d\n", i, ret);
      status = TEST_FAIL;
    }

    vulnspec_init(&interp);
    ret = vulnspec_load(&interp, p.progn.buf.data, p.progn.buf.len);
    if (ret != 0) {
      TEST_LOGF("index:%zu vulnspec_validate %d\n", i, ret);
      status = TEST_FAIL;
    }

    if (debug) {
      fprintf(stderr, "%s\n  =>\n", tests[i].input);
      hexdump(stderr, p.progn.buf.data, p.progn.buf.len);
      fprintf(stderr, "\n\n");
    }

    fclose(fp);
parser_cleanup:
    vulnspec_parser_cleanup(&p);
  }

  return status;
}

static int eval_one_cb(struct vulnspec_cve_match *m, void *data) {
  const char **wiie = data;

  *wiie = m->id;
  return 1;
}

static int test_eval_one() {
  int status = TEST_OK;
  size_t i;
  static struct {
    const char *vendprod;
    const char *version;
    char *prog;
    const char *expected_cve;
  } tests[] = {
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (< \"foo/bar\" \"1.2.4\"))",
      "my-cve"
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (< \"foo/bar\" \"1.2.3\"))",
      NULL
    },
    {
      "foo/bar",
      "",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (< \"foo/bar\" \"1.2.3\"))",
      NULL
    },
    {
      "foo/bar",
      "",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (> \"foo/bar\" \"1.2.3\"))",
      NULL
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (<= \"foo/bar\" \"1.2.3\"))",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (= \"foo/bar\" \"1.2.3.4\"))",
      NULL,
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (= \"foo/bar\" \"1.2.3\"))",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (= \"foo/bar\" \"1.2.4\"))",
      NULL,
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (>= \"foo/bar\" \"1.2.3\"))",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (> \"foo/bar\" \"1.2.3\"))",
      NULL,
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (> \"foo/bar\" \"1.2.2\"))",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (^\n"
      "    (> \"foo/bar\" \"1.2.2\")\n"
      "    (< \"foo/bar\" \"1.2.4\")))\n",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.4",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (^\n"
      "    (> \"foo/bar\" \"1.2.2\")\n"
      "    (< \"foo/bar\" \"1.2.4\")))\n",
      NULL,
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (v\n"
      "    (^\n"
      "      (> \"bar/foo\" \"1.2.2\")\n"
      "      (< \"bar/foo\" \"1.2.4\"))\n"
      "    (^\n"
      "      (> \"foo/bar\" \"1.2.2\")\n"
      "      (< \"foo/bar\" \"1.2.4\"))))\n",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.4",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (v\n"
      "    (^\n"
      "      (> \"bar/foo\" \"1.2.2\")\n"
      "      (< \"bar/foo\" \"1.2.4\"))\n"
      "    (^\n"
      "      (> \"foo/bar\" \"1.2.2\")\n"
      "      (< \"foo/bar\" \"1.2.4\"))))\n",
      NULL,
    },
    {
      "foo/bar",
      "1.2.4",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (v\n"
      "    (^\n"
      "      (> \"bar/foo\" \"1.2.2\")\n"
      "      (<= \"bar/foo\" \"1.2.4\"))\n"
      "    (^\n"
      "      (> \"foo/bar\" \"1.2.2\")\n"
      "      (< \"foo/bar\" \"1.2.4\"))))\n",
      NULL,
    },
    {
      "foo/bar",
      "1.2.4",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (v\n"
      "    (^\n"
      "      (> \"bar/foo\" \"1.2.2\")\n"
      "      (<= \"bar/foo\" \"1.2.4\"))\n"
      "    (^\n"
      "      (> \"foo/bar\" \"1.2.2\")\n"
      "      (<= \"foo/bar\" \"1.2.4\"))))\n",
      "my-cve",
    },
    /* start nalpha tests */
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (= \"foo/bar\" \"1.2.3r\"))",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.3r",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (= \"foo/bar\" \"1.2.3\"))",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.3r",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (= \"foo/bar\" \"1.2.3r\"))",
      "my-cve",
    },
    {
      "foo/bar",
      "1.2.3",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (nalpha (= \"foo/bar\" \"1.2.3r\")))",
      NULL,
    },
    {
      "foo/bar",
      "1.2.3r",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (nalpha (= \"foo/bar\" \"1.2.3\")))",
      NULL,
    },
    {
      "foo/bar",
      "1.2.3r",
      "(cve \"my-cve\" 6.5 6.5 \"bar\"\n"
      "  (nalpha (= \"foo/bar\" \"1.2.3r\")))",
      "my-cve",
    },
    /* end nalpha tests */
  };
  int ret;
  struct vulnspec_parser p;
  struct vulnspec_interp interp;
  FILE *fp;
  char *envdbg;
  int debug = 0;
  char *cve;

  envdbg = getenv("TEST_DEBUG");
  if (envdbg != NULL && *envdbg == '1') {
    debug = 1;
  }

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    ret = vulnspec_parser_init(&p);
    if (ret != 0) {
      TEST_LOGF("index:%zu vulnspec_parser_init failure\n", i);
      status = TEST_FAIL;
      continue;
    }

    fp = fmemopen(tests[i].prog, strlen(tests[i].prog), "rb");
    if (!fp) {
      TEST_LOGF("index:%zu fmemopen failure", i);
      status = TEST_FAIL;
      goto parser_cleanup;
    }

    ret = vulnspec_parse(&p, fp);
    if (ret != 0) {
      TEST_LOGF("index:%zu vulnspec_parse %d\n", i, ret);
      status = TEST_FAIL;
      goto fclose_fp;
    }

    vulnspec_init(&interp);
    ret = vulnspec_load(&interp, p.progn.buf.data, p.progn.buf.len);
    if (ret != 0) {
      TEST_LOGF("index:%zu vulnspec_validate %d\n", i, ret);
      status = TEST_FAIL;
      goto fclose_fp;
    }

    if (debug) {
      fprintf(stderr, "%s\n  =>\n", tests[i].prog);
      hexdump(stderr, p.progn.buf.data, p.progn.buf.len);
      fprintf(stderr, "\n\n");
    }

    cve = NULL;
    vulnspec_set(&interp, VULNSPEC_PCVEVENDPROD, tests[i].vendprod);
    vulnspec_set(&interp, VULNSPEC_PCVEVERSION, tests[i].version);
    vulnspec_set(&interp, VULNSPEC_PCVEONMATCH, &eval_one_cb);
    vulnspec_set(&interp, VULNSPEC_PCVEONMATCHDATA, &cve);
    ret = vulnspec_eval(&interp);
    if (ret != 0) {
      TEST_LOGF("index:%zu vulnspec_eval %d\n", i, ret);
      status = TEST_FAIL;
      goto fclose_fp;
    }

    if (cve == NULL && tests[i].expected_cve != NULL) {
      TEST_LOGF("index:%zu cve: expected %s, was NULL", i,
          tests[i].expected_cve);
      status = TEST_FAIL;
    } else if (cve != NULL && tests[i].expected_cve == NULL) {
      TEST_LOGF("index:%zu cve: expected NULL, was %s\n", i, cve);
      status = TEST_FAIL;
    } else if (cve != NULL && tests[i].expected_cve != NULL) {
      if (strcmp(cve, tests[i].expected_cve) != 0) {
        TEST_LOGF("index:%zu cve: expected %s, was %s\n", i,
            tests[i].expected_cve, cve);
        status = TEST_FAIL;
      }
    }

fclose_fp:
    fclose(fp);
parser_cleanup:
    vulnspec_parser_cleanup(&p);
  }

  return status;
}

static int on_match(struct vulnspec_cve_match *m, void *data) {
  return 1;
}

static int test_eval_noload() {
  struct vulnspec_interp p;
  int ret;

  vulnspec_init(&p);
  vulnspec_set(&p, VULNSPEC_PCVEVENDPROD, "foo/bar");
  vulnspec_set(&p, VULNSPEC_PCVEVERSION, "1.2.3");
  vulnspec_set(&p, VULNSPEC_PCVEONMATCH, &on_match);
  ret = vulnspec_eval(&p);
  vulnspec_unloadfile(&p);
  return ret == 0 ? TEST_OK : TEST_FAIL;
}


TEST_ENTRY(
  {"read_token", test_read_token},
  {"parse_ok", test_parse_ok},
  {"eval_one", test_eval_one},
  {"eval_noload", test_eval_noload},
);
