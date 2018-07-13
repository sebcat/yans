#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/util/str.h>

#define MAX_MAP_FIELDS 8

struct str_map_fieldz_data {
  const char *input;
  const char *seps;
  size_t nexpected;
  const char *expected[MAX_MAP_FIELDS];
  size_t curr_expected_index;
};

static int mapfunc(const char *s, size_t len, void *data) {
  struct str_map_fieldz_data *curr = data;
  size_t expected_len;

  if (len == sizeof("@STOP@")-1 && strncmp(s, "@STOP@", len) == 0) {
    return 0;
  }

  if (curr->curr_expected_index >= curr->nexpected) {
    fprintf(stderr, "%zu: unexpected trailing string: \"%.*s\"\n",
        curr->curr_expected_index, (int)len, s);
    return -1;
  }

  expected_len = strlen(curr->expected[curr->curr_expected_index]);
  if (expected_len != len) {
    fprintf(stderr, "%zu: expected string length %zu, got %zu\n",
        curr->curr_expected_index, expected_len, len);
    return -1;
  }

  if (strncmp(s, curr->expected[curr->curr_expected_index], len) != 0) {
    fprintf(stderr, "%zu: expected string \"%s\", got \"%.*s\"\n",
        curr->curr_expected_index,
        curr->expected[curr->curr_expected_index], (int)len, s);
    return -1;
  }

  curr->curr_expected_index++;
  return 1;
}

static int test_str_map_fieldz() {
  struct str_map_fieldz_data tests[] = {
    {
      .input = "",
      .seps = "\r\n\t ",
      .nexpected = 0,
      .expected = {0},
    },
    {
      .input = "f",
      .seps = "\r\n\t ",
      .nexpected = 1,
      .expected = {"f"},
    },
    {
      .input = "foo",
      .seps = "\r\n\t ",
      .nexpected = 1,
      .expected = {"foo"},
    },
    {
      .input = " foobar",
      .seps = "\r\n\t ",
      .nexpected = 1,
      .expected = {"foobar"},
    },
    {
      .input = "foobar ",
      .seps = "\r\n\t ",
      .nexpected = 1,
      .expected = {"foobar"},
    },
    {
      .input = " foobar ",
      .seps = "\r\n\t ",
      .nexpected = 1,
      .expected = {"foobar"},
    },
    {
      .input = " foobar k",
      .seps = "\r\n\t ",
      .nexpected = 2,
      .expected = {"foobar", "k"},
    },
    {
      .input = " foobar\tk",
      .seps = "\r\n\t ",
      .nexpected = 2,
      .expected = {"foobar", "k"},
    },
    {
      .input = " foobar\tk ",
      .seps = "\r\n\t ",
      .nexpected = 2,
      .expected = {"foobar", "k"},
    },
    {
      .input = " foobar\tk wiie",
      .seps = "\r\n\t ",
      .nexpected = 3,
      .expected = {"foobar", "k", "wiie"},
    },
    {
      .input = " foobar\tk @STOP@ wiie",
      .seps = "\r\n\t ",
      .nexpected = 2,
      .expected = {"foobar", "k"},
    },
  };
  struct str_map_fieldz_data *curr;
  size_t i;
  int ret;
  int result = EXIT_SUCCESS;

  for (i = 0; i < (sizeof(tests) / sizeof(struct str_map_fieldz_data)); i++) {
    curr = &tests[i];
    ret = str_map_fieldz(curr->input, curr->seps, mapfunc, curr);
    if (ret < 0) {
      /* mapfunc should print what's wrong, if anything */
      result = EXIT_FAILURE;
    } else if (curr->nexpected != curr->curr_expected_index) {
      fprintf(stderr, "expected %zu fields, got %zu\n", curr->nexpected,
          curr->curr_expected_index);
      result = EXIT_FAILURE;
    }
  }

  return result;
}

#include <lib/util/str.h>

int main() {
  int ret = EXIT_SUCCESS;
  size_t i;
  struct {
    char *name;
    int (*func)(void);
  } tests[] = {
    {"str_map_fieldz", test_str_map_fieldz},
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
