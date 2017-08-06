#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lib/util/os.h>

int test_cleanpath() {
  struct {
    char *input;
    char *expected;
  } vals[] = {
    {"", ""},
    {".", ""},
    {"..", ".."},
    {"../", ".."},
    {"../..", "../.."},
    {"../../", "../.."},
    {"./", ""},
    {"/.", "/"},
    {"/..", "/"},
    {"/../", "/"},
    {"/../..", "/"},
    {"/../../", "/"},
    {"//", "/"},
    {"///", "/"},
    {"///..", "/"},
    {"///../", "/"},
    {"///../.", "/"},
    {"///../..", "/"},
    {"///..//.", "/"},
    {"///..//..", "/"},
    {"foo", "foo"},
    {"foo/", "foo"},
    {"foo/.", "foo"},
    {"foo/./", "foo"},
    {"foo/..", ""},
    {"foo/../", ""},
    {"/foo", "/foo"},
    {"/foo/", "/foo"},
    {"/foo/.", "/foo"},
    {"/foo/./", "/foo"},
    {"/foo/..", "/"},
    {"/foo/../", "/"},
    {"/foo/../..", "/"},
    {"/foo/../../", "/"},
    {"/foo/bar/baz/../../", "/foo"},
    {"foo/bar/baz/../../", "foo"},
    {"/foo/bar/baz/../../..", "/"},
    {"foo/bar/baz/../../..", ""},

    {NULL, NULL},
  };
  size_t i;
  char *actual;
  int ret = EXIT_FAILURE;

  for(i = 0; vals[i].input != NULL; i++) {
    actual = strdup(vals[i].input);
    if (actual == NULL) {
      perror("strdup");
      goto done;
    }

    os_cleanpath(actual);
    if (strcmp(vals[i].expected, actual) != 0) {
      fprintf(stderr, "  input:\"%s\" expected:\"%s\" actual:\"%s\"\n",
          vals[i].input, vals[i].expected, actual);
      free(actual);
      goto done;
    }
    free(actual);
  }

  ret = EXIT_SUCCESS;
done:
  return ret;
}

int main() {
  struct {
    char *name;
    int (*callback)(void);
  } tests[] = {
    {"cleanpath", test_cleanpath},
    {NULL, NULL},
  };
  size_t i;

  for(i = 0; tests[i].name != NULL; i++) {
    if (tests[i].callback() != EXIT_SUCCESS) {
      fprintf(stderr, "FAIL: %s\n", tests[i].name);
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

