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

#include <lib/util/test.h>
#include <lib/util/macros.h>
#include <lib/net/urlquery.h>

static int test_decode() {
  static const struct {
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
      TEST_LOGF("expected:\"%s\" actual:\"%s\"", tests[i].expected,
          tmp);
    }
    free(tmp);
  }

  return result;
}

#define MAX_PAIRS 32

static int test_next_pair() {
  size_t i;
  size_t pair;
  char *str;
  char *curr;
  char *key;
  char *val;
  int result = TEST_OK;
  int ret;
  static const struct {
    const char *input;
    struct {
      size_t npairs;
      const char *keys[MAX_PAIRS];
      const char *vals[MAX_PAIRS];
    } expected;
  } tests[] = {
    {
      .input="",
      .expected = {
        .npairs = 0,
        .keys   = {0},
        .vals   = {0},
      }
    },
    {
      .input="foo",
      .expected = {
        .npairs = 1,
        .keys   = {"foo"},
        .vals   = {""},
      }
    },
    {
      .input="foo=",
      .expected = {
        .npairs = 1,
        .keys   = {"foo"},
        .vals   = {""},
      }
    },
    {
      .input="&foo=",
      .expected = {
        .npairs = 1,
        .keys   = {"foo"},
        .vals   = {""},
      }
    },
    {
      .input="foo=bar",
      .expected = {
        .npairs = 1,
        .keys   = {"foo"},
        .vals   = {"bar"},
      }
    },
    {
      .input="&foo=bar",
      .expected = {
        .npairs = 1,
        .keys   = {"foo"},
        .vals   = {"bar"},
      }
    },
    {
      .input="&foo=bar&",
      .expected = {
        .npairs = 1,
        .keys   = {"foo"},
        .vals   = {"bar"},
      }
    },
    {
      .input="&foo=bar=baz",
      .expected = {
        .npairs = 1,
        .keys   = {"foo"},
        .vals   = {"bar=baz"},
      }
    },
    {
      .input="%C3%A5%C3%A4%C3%B6=xyz",
      .expected = {
        .npairs = 1,
        .keys   = {"åäö"},
        .vals   = {"xyz"},
      }
    },
    {
      .input="foo=bar&baz=foobar",
      .expected = {
        .npairs = 2,
        .keys   = {"foo", "baz"},
        .vals   = {"bar", "foobar"},
      }
    },
  };

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    str = curr = strdup(tests[i].input);
    for (pair = 0; pair < tests[i].expected.npairs; pair++) {
      ret = urlquery_next_pair(&curr, &key, &val);
      if (ret != 1) {
        TEST_LOGF("expected ret:1 actual:%d test:%zu pair:%zu",
            ret, i, pair);
        result = TEST_FAIL;
      } else if (key == NULL ||
          strcmp(key, tests[i].expected.keys[pair]) != 0) {
        TEST_LOGF("expected key:\"%s\" actual:\"%s\" "
             "test:%zu pair:%zu",
            tests[i].expected.keys[pair], key ? key : "(null)", i, pair);
        result = TEST_FAIL;
      } else if (val == NULL && tests[i].expected.vals[pair] != NULL) {
        TEST_LOGF("expected value:\"%s\" was:(null) test:%zu pair:%zu",
            tests[i].expected.vals[pair], i, pair);
        result = TEST_FAIL;
      } else if (val != NULL && tests[i].expected.vals[pair] == NULL) {
        TEST_LOGF("expected NULL value, was:\"%s\" test:%zu pair:%zu",
            val, i, pair);
        result = TEST_FAIL;
      } else if (val != NULL && tests[i].expected.vals[pair] != NULL &&
          strcmp(val, tests[i].expected.vals[pair]) != 0) {
        TEST_LOGF("expected value:\"%s\", actual:\"%s\" "
            "test:%zu pair:%zu",tests[i].expected.vals[pair], val, i,
            pair);
        result = TEST_FAIL;
      }
    }

    if (*curr != '\0') {
      result = TEST_FAIL;
      TEST_LOG("expected to end up at end of string");
    }

    /* do one moar, expect zero return and NULL key */
    ret = urlquery_next_pair(&curr, &key, &val);
    if (ret != 0) {
      result = TEST_FAIL;
      TEST_LOGF("expected 0 return at end, was:%d", ret);
    } else if (key != NULL) {
      result = TEST_FAIL;
      TEST_LOG("expected NULL key at end");
    }

    free(str);
  }

  return result;
}

TEST_ENTRY(
  {"decode", test_decode},
  {"next_pair", test_next_pair},
);
