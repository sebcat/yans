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

#include <lib/util/vaguever.h>
#include <lib/util/macros.h>
#include <lib/util/test.h>

static int test_compare() {
  int result = TEST_OK;
  int val;
  size_t i;
  struct vaguever_version v1;
  struct vaguever_version v2;
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
  };

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    vaguever_init(&v1, tests[i].v1);
    vaguever_init(&v2, tests[i].v2);
    val = vaguever_cmp(&v1, &v2);
    if (val != tests[i].expected) {
      TEST_LOGF("entry %zu: expected %d, was: %d", i, tests[i].expected,
          val);
      result = TEST_FAIL;
    }
  }

  return result;
}

int test_parse() {
  size_t i;
  struct {
    const char *input;
    const char *expected;
  } tests[] = {
    {"", "0"},
    {"0", "0"},
    {"00", "0"},
    {"01", "1"},
    {"10", "10"},
    {"11", "11"},
    {"0.", "0"},
    {"0.1", "0.1"},
    {"0.1.", "0.1"},
    {"0.1.2", "0.1.2"},
    {"0.1.2.", "0.1.2"},
    {"0.1.2.3", "0.1.2.3"},
    {"0.1.2.3.", "0.1.2.3"},
    {"0.1.2.3.4", "0.1.2.3"},
    {"7.2p666", "7.2"}, /* FYI regarding vaguever OpenSSH behavior */
    {"1.0.1", "1.0.1"},
    {"0.1.0", "0.1.0"},
    {"0.1.0", "0.1.0"},
    {"0..1", "0.1"},
    {"0...1", "0.1"},
  };
  struct vaguever_version v;
  char buf[64];
  int result = TEST_OK;

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    vaguever_init(&v, tests[i].input);
    vaguever_str(&v, buf, sizeof(buf));
    if (strcmp(tests[i].expected, buf) != 0) {
      TEST_LOGF("input:\"%s\" expected:\"%s\", actual:\"%s\"\n",
          tests[i].input, tests[i].expected, buf);
      result = TEST_FAIL;
    }
  }

  return result;
}

TEST_ENTRY(
  {"parse", test_parse},
  {"compare", test_compare},
);
