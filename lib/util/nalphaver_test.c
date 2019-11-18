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
#include "lib/util/macros.h"
#include "lib/util/nalphaver.h"
#include "lib/util/test.h"

static int test_compare() {
  int result = TEST_OK;
  int val;
  size_t i;
  struct {
    const char *v1;
    const char *v2;
    int expected;
  } tests[] = {
    {"1.0", "1.0", 0},
    {"1.0", "1", 0},
    {"1", "1.0", 0},
    {"1", "1", 0},
    {"1.1", "1.0", 1},
    {"1.1.1", "1.1", 1},
    {"1.1", "1.0.1", 1},
    {"1.0", "1.1", -1},
    {"1.1", "1.1.1", -1},
    {"1.0.1", "1.1", -1},
    {"1.9", "1.1", 1},
    {"1.9", "1.10", -1},
    {"1.0.1", "1.0.1a", -1},
    {"1.0.1a", "1.0.1b", -1},
    {"1.0.1z", "1.0.1b", 1},
    {"1.0.1k", "1.0.1k", 0},
    {"1.0p1", "1.0p9", -1}, /* let's hope there's only ten 'p' releases */
    {"1.0p9", "1.0p1", 1},
    {"1.0p1", "1.0p1", 0},
  };

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    val = nalphaver_cmp(tests[i].v1, tests[i].v2);
    if (val != tests[i].expected) {
      TEST_LOGF("entry %zu: expected %d, was: %d", i, tests[i].expected,
          val);
      result = TEST_FAIL;
    }
  }

  return result;
}

TEST_ENTRY(
  {"compare", test_compare},
);
