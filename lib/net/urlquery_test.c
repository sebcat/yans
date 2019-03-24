#include <string.h>

#include <lib/util/test.h>
#include <lib/util/macros.h>
#include <lib/net/urlquery.h>

static int test_decode() {
  struct {
    const char *input;
    const char *expected;
  } tests[] = {
    {"", ""},
    {"x", "x"},
    {"åäö", "åäö"},
    {"%C3%A5%C3%A4%C3%B6", "åäö"},
    {"%c3%a5%C3%A4%C3%b6", "åäö"},
    {"foo+bar", "foo bar"},
    {"foo%20bar", "foo bar"},
    {"foo%20%25bar", "foo %bar"},
    {"foo%xbar", "foo%xbar"},
    {"foobar%x", "foobar%x"},
  };
  size_t i;
  char *tmp;
  int result = TEST_OK;

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    tmp = strdup(tests[i].input);
    urlquery_decode(tmp);
    if (strcmp(tmp, tests[i].expected) != 0) {
      result = TEST_FAIL;
      TEST_LOG_ERRF("expected:\"%s\" actual:\"%s\"", tests[i].expected,
          tmp);
    }
    free(tmp);
  }

  return result;
}

TEST_ENTRY(
  {"decode", test_decode},
);
