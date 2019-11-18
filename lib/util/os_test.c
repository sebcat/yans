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

