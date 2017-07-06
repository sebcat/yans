#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <lib/util/conf.h>

struct myconf {
  const char *str_val;
  unsigned long long_val;
  const char *another_str;
};

struct conf_map m[] = {
  CONF_MENTRY(struct myconf, str_val, CONF_TSTR),
  CONF_MENTRY(struct myconf, long_val, CONF_TULONG),
  CONF_MENTRY(struct myconf, another_str, CONF_TSTR),
  CONF_MENTRY_END,
};

static int test_parse_expect_failure() {
  return EXIT_SUCCESS;
}

static bool teq(struct myconf *expected, struct myconf *actual) {
  if (expected->str_val == NULL && actual->str_val != NULL) {
    return false;
  } else if (expected->str_val != NULL && actual->str_val == NULL) {
    return false;
  } else if (expected->str_val && actual->str_val &&
      strcmp(expected->str_val, actual->str_val) != 0) {
    return false;
  }

  if (expected->long_val != actual->long_val) {
    return false;
  }

  if (expected->another_str == NULL && actual->another_str != NULL) {
    return false;
  } else if (expected->another_str != NULL && actual->another_str == NULL) {
    return false;
  } else if (expected->another_str && actual->another_str &&
      strcmp(expected->another_str, actual->another_str) != 0) {
    return false;
  }

  return true;
}

static int test_parse_expect_success() {
  size_t i;
  struct myconf actual;
  struct conf cfg;
  struct {
    const char *input;
    struct myconf expected;
  } tests[] = {
    {
      .input =
          "\r\n\t#str_val: foo\r\n",
      .expected = {
        .str_val = NULL,
        .long_val = 0,
        .another_str = NULL,
      }
    },
    {
      .input =
          "\r\n\tstr_val:faa\r\n",
      .expected = {
        .str_val = "faa",
        .long_val = 0,
        .another_str = NULL,
      }
    },
    {
      .input =
          "\r\n\tstr_val: foo\r\n",
      .expected = {
        .str_val = "foo",
        .long_val = 0,
        .another_str = NULL,
      }
    },
    {
      .input =
          "\r\n\tstr_val :fii\r\n",
      .expected = {
        .str_val = "fii",
        .long_val = 0,
        .another_str = NULL,
      }
    },
    {
      .input =
          "\r\n\tanother_str: \r\n",
      .expected = {
        .str_val = NULL,
        .long_val = 0,
        .another_str = NULL,
      }
    },
    {
      .input =
          "\r\n\tanother_str: kex\r\n",
      .expected = {
        .str_val = NULL,
        .long_val = 0,
        .another_str = "kex",
      }
    },
    {
      .input =
          "\r\n\t#str_val: foo\r\nlong_val : 21\n",
      .expected = {
        .str_val = NULL,
        .long_val = 21,
        .another_str = NULL,
      }
    },
    {
      .input =
          "\r\n\t#str_val: foo\r\nlong_val : 21\nlong_val:22\n",
      .expected = {
        .str_val = NULL,
        .long_val = 22,
        .another_str = NULL,
      }
    },
    {
      .input =
          "\r\n\t#str_val: foo\r\nlong_val : 21\nlong_val:22\n",
      .expected = {
        .str_val = NULL,
        .long_val = 22,
        .another_str = NULL,
      }
    },
    {
      .input =
        "str_val: wolo\n"
        "long_val : 42\n"
        "another_str:\\\nfoo\\\nbar\n",
      .expected = {
        .str_val = "wolo",
        .long_val = 42,
        .another_str = "foo\nbar",
      }
    },
    {
      .input =
        "# this is a typical config file\n"
        "# there's some comments\n"
        "\n"
        "str_val: wolo\n"
        "\n"
        " #describing comment\n"
        "long_val : 42\n"
        "another_str: foo\\\nbar\n",
      .expected = {
        .str_val = "wolo",
        .long_val = 42,
        .another_str = "foo\nbar",
      }
    },
    {0},
  };

  for (i=0; tests[i].input != NULL; i++) {
    if (conf_init_from_str(&cfg, tests[i].input) < 0) {
      fprintf(stderr, "conf_init_from_str failure: %s (index: %zu)\n",
          conf_strerror(&cfg), i);
      return EXIT_FAILURE;
    }

    memset(&actual, 0, sizeof(actual));
    if (conf_parse(&cfg, m, &actual) < 0) {
      fprintf(stderr, "conf_init_from_str failure: %s (index: %zu)\n",
          conf_strerror(&cfg), i);
      return EXIT_FAILURE;
    }

    if (!teq(&tests[i].expected, &actual)) {
      fprintf(stderr,
          "index:    %zu\n"
          "expected: {str_val:%s, long_val:%lu, another_str:%s}\n"
          "got:      {str_val:%s, long_val:%lu, another_str:%s}\n",
          i,
          tests[i].expected.str_val == NULL ? "null" :
              tests[i].expected.str_val,
          tests[i].expected.long_val,
          tests[i].expected.another_str == NULL ? "null" :
              tests[i].expected.another_str,
          actual.str_val == NULL ? "null" : actual.str_val,
          actual.long_val,
          actual.another_str == NULL ? "null" : actual.another_str);
      return EXIT_FAILURE;
    }

    conf_cleanup(&cfg);
  }

  return EXIT_SUCCESS;
}

int main() {
  int ret = EXIT_SUCCESS;
  size_t i;
  struct {
    const char *name;
    int (*cb)(void);
  } *t, tests[] = {
    {"parse_expect_failure", test_parse_expect_failure},
    {"parse_expect_success", test_parse_expect_success},
    {NULL, NULL},
  };

  for(i=0; tests[i].name != NULL; i++) {
    t = &tests[i];
    if (t->cb() == 0) {
      fprintf(stderr, "OK  %s\n", t->name);
    } else {
      fprintf(stderr, "ERR %s\n", t->name);
      ret = EXIT_FAILURE;
    }
  }

  return ret;
}

