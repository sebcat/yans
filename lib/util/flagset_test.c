#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <lib/util/flagset.h>


static int test_parse() {
  static const struct flagset_map m[] = {
    FLAGSET_ENTRY("foo", (1 << 0)),
    FLAGSET_ENTRY("bar", (1 << 1)),
    FLAGSET_ENTRY("baz", (1 << 2)),
    FLAGSET_END,
  };
  struct test_parse_inputs {
    const char *input;
    unsigned int expected;
  } vals[] = {
    {NULL, 0},
    {"", 0},
    {"foo", (1 << 0)},
    {"foo|", (1 << 0)},
    {"foo|bar", (1 << 0) | (1 << 1)},
    {"foo|bar|", (1 << 0) | (1 << 1)},
    {"foo|bar|baz", (1 << 0) | (1 << 1) | (1 << 2)},
    {"foo|bar|baz|", (1 << 0) | (1 << 1) | (1 << 2)},
    {"foo|baz|", (1 << 0) | (1 << 2)},
    {"foo||baz|", (1 << 0) | (1 << 2)},
    {"foo| \t |baz|", (1 << 0) | (1 << 2)},
    {"baz| \t |foo|", (1 << 0) | (1 << 2)},
  };
  size_t i;
  int res = EXIT_SUCCESS;
  int ret;
  struct flagset_result actual;

  for(i=0; i < (sizeof(vals) / sizeof(struct test_parse_inputs)); i++) {
    memset(&actual, 0, sizeof(actual));
    ret = flagset_from_str(m, vals[i].input, &actual);
    if (ret < 0) {
      fprintf(stderr, "input:\"%s\" error:%s offset:%zu\n", vals[i].input,
          actual.errmsg, actual.erroff);
      res = EXIT_FAILURE;
      continue;
    }

    if (vals[i].expected != actual.flags) {
      fprintf(stderr, "input:\"%s\" expected:%x actual:%x\n", vals[i].input,
          vals[i].expected, actual.flags);
      res = EXIT_FAILURE;
    }
  }

  return res;
}

static int test_parse_failure() {
  static const struct flagset_map m[] = {
    FLAGSET_ENTRY("foo", (1 << 0)),
    FLAGSET_ENTRY("bar", (1 << 1)),
    FLAGSET_ENTRY("baz", (1 << 2)),
    FLAGSET_END,
  };
  struct test_parse_failure_inputs {
    const char *input;
  } vals[] = {
    {"fo"},
    {"foo|ba"},
    {"baz|bar|fo"},
    {NULL},
  };
  size_t i;
  int res = EXIT_SUCCESS;
  int ret;
  struct flagset_result actual;

  for(i=0; vals[i].input != NULL; i++) {
    memset(&actual, 0, sizeof(actual));
    ret = flagset_from_str(m, vals[i].input, &actual);
    if (ret == 0) {
      fprintf(stderr, "input:\"%s\" expected failure, got: %x\n",
          vals[i].input, actual.flags);
      res = EXIT_FAILURE;
    }
  }

  return res;
}

int main() {
  int ret = EXIT_SUCCESS;
  size_t i;
  struct {
    char *name;
    int (*func)(void);
  } tests[] = {
    {"parse", test_parse},
    {"parse_failure", test_parse_failure},
    {NULL, NULL},
  };

  for (i = 0; tests[i].name != NULL; i++) {
    if (tests[i].func() == EXIT_SUCCESS) {
      fprintf(stderr, "OK  %s\n", tests[i].name);
    } else {
      fprintf(stderr, "ERR %s\n", tests[i].name);
      ret = EXIT_FAILURE;
    }
  }

  return ret;
}
