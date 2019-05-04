#include <string.h>

#include <lib/match/reset.h>
#include <lib/util/macros.h>
#include <lib/util/test.h>

int test_match() {
  int status = TEST_OK;
  static const struct {
    const char *re;
    const char *data;
    size_t len;
  } inputs[] = {
    {"", ""},
    {"", "trololo"},
    {"foo", "foo"},
    {"bar", "foo\0bar", sizeof("foo\0bar")-1},
  };
  size_t i;
  int ret;
  int id;
  int found;
  reset_t *reset;

  reset = reset_new();
  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    ret = reset_add(reset, inputs[i].re);
    if (ret != (int)i) {
      status = TEST_FAIL;
      TEST_LOGF("reset_add ID mismatch i:%zu %s", i, reset_strerror(reset));
    }
  }

  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("%s", reset_strerror(reset));
  }

  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    ret = reset_match(reset, inputs[i].data,
        inputs[i].len == 0 ? strlen(inputs[i].data) : inputs[i].len);
    if (ret != RESET_OK) {
      status = TEST_FAIL;
      TEST_LOGF("i:%zu %s", i, reset_strerror(reset));
    }

    found = 0;
    while ((id = reset_get_next_match(reset)) >= 0) {
      if (id == (int)i) {
        found = 1;
        break;
      }
    }

    if (!found) {
      status = TEST_FAIL;
      TEST_LOGF("i:%zu mismatch", i);
    }
  }

  reset_free(reset);
  return status;
}

int test_noadd() {
  int status = TEST_OK;
  static const struct {
    const char *re;
  } inputs[] = {
    {"foo["},
    {"foo\\"},
    {"foo("},
  };
  size_t i;
  int ret;
  reset_t *reset;

  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    reset = reset_new();
    ret = reset_add(reset, inputs[i].re);
    if (ret == RESET_ERR) {
      TEST_LOGF("expected failure i:%zu %s", i, reset_strerror(reset));
    } else {
      TEST_LOGF("unexpected success i:%zu", i);
      status = TEST_FAIL;
    }

    reset_free(reset);
  }

  return status;
}

int test_nomatch() {
  int status = TEST_OK;
  static const struct {
    const char *re;
    const char *data;
    size_t len;
  } inputs[] = {
    {"foo", ""},
    {"foo", "bar"},
    {"baz", "foo\0bar", sizeof("foo\0bar")-1},
  };
  size_t i;
  int ret;
  int id;
  int found;
  reset_t *reset;

  reset = reset_new();
  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    ret = reset_add(reset, inputs[i].re);
    if (ret != (int)i) {
      status = TEST_FAIL;
      TEST_LOGF("reset_add ID mismatch i:%zu %s", i, reset_strerror(reset));
    }
  }

  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("%s", reset_strerror(reset));
  }

  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    reset_match(reset, inputs[i].data,
        inputs[i].len == 0 ? strlen(inputs[i].data) : inputs[i].len);
    found = 0;
    while ((id = reset_get_next_match(reset)) >= 0) {
      if (id == (int)i) {
        found = 1;
        break;
      }
    }

    if (found) {
      status = TEST_FAIL;
      TEST_LOGF("i:%zu unexpected match", i);
    }
  }

  reset_free(reset);
  return status;
}

int test_substrings() {
  int status = TEST_OK;
  reset_t *reset;
  int ret;
  int id;
  const char data[] = "foo adam bar bertil baz cesar";
  const char *sub;

  reset = reset_new();
  reset_add(reset, "foo ([a-z]+)"); /* 0 */
  reset_add(reset, "bar ([a-z]+)"); /* 1 */
  reset_add(reset, "baz ([a-z]+)"); /* 2 */
  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("%s", reset_strerror(reset));
  }

  reset_match(reset, data, sizeof(data)-1);
  while ((id = reset_get_next_match(reset)) >= 0) {
    sub = reset_get_substring(reset, id, data, sizeof(data)-1, NULL);
    if (sub == NULL) {
      status = TEST_FAIL;
      TEST_LOGF("missing substring for id %d", id);
      continue;
    }

    switch (id) {
    case 0:
      if (strcmp(sub, "adam") != 0) {
        status = TEST_FAIL;
        TEST_LOGF("expected adam, got %s", sub);
      }
      break;
    case 1:
      if (strcmp(sub, "bertil") != 0) {
        status = TEST_FAIL;
        TEST_LOGF("expected bertil, got %s", sub);
      }
      break;
    case 2:
      if (strcmp(sub, "cesar") != 0) {
        status = TEST_FAIL;
        TEST_LOGF("expected cesar, got %s", sub);
      }
      break;
    default:
      status = TEST_FAIL;
      TEST_LOGF("unexpected id: %d", id);
    }
  }

  reset_free(reset);
  return status;
}

TEST_ENTRY(
  {"match", test_match},
  {"noadd", test_noadd},
  {"nomatch", test_nomatch},
  {"substrings", test_substrings},
)
