#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <lib/util/macros.h>
#include <lib/util/u8.h>
#include <lib/util/test.h>

__attribute__((constructor)) static void init_locale() {
  setlocale(LC_ALL, "");
}

static int test_u8_to_from_cp() {
  struct {
    char *s;
    size_t len;
    int32_t expected;
  } tests[] = {
    {"\x00", 1, 0},
    {"\x7f", 1, 0x7f},
    {"\xc2\x80", 2, 0x80},
    {"\xdf\xbf", 2, 0x07ff},
    {"\xe0\xa0\x80", 3, 0x0800},
    {"\xe2\x98\x83", 3, 0x2603},
    {"\xef\xbf\xbf", 3, 0xffff},
    {"\xf0\x90\x80\x80", 4, 0x010000},
    {"\xf4\x80\x83\xbf", 4, 0x1000ff},
    {NULL, 0, 0},
  };
  size_t i;
  size_t tolen = 0xff;
  size_t fromlen = 0xff;
  int32_t actual;
  int status = TEST_OK;
  int ret;
  char buf[8];

  for(i=0; tests[i].s != NULL; i++) {
    /* UTF-8 -> codepoint */
    actual = u8_to_cp(tests[i].s, tests[i].len, &tolen);
    if (tests[i].expected != actual) {
      status = TEST_FAIL;
      TEST_LOGF("index:%zu expected:0x%08x actual:0x%08x", i,
          tests[i].expected, actual);
    }

    /* codepoint -> UTF-8*/
    memset(buf, 0, sizeof(buf));
    ret = u8_from_cp(buf, sizeof(buf), actual, &fromlen);
    if (ret < 0) {
      status = TEST_FAIL;
      TEST_LOGF("index:%zu return:%d\n", i, ret);
    } else if (memcmp(tests[i].s, buf, tests[i].len + 1) != 0) {
      status = TEST_FAIL;
      TEST_LOGF("index:%zu mismatched", i);
    } else if (tolen != fromlen) {
      status = TEST_FAIL;
      TEST_LOGF("index:%zu tolen:%zu fromlen:%zu", i, tolen, fromlen);
    }
  }

  return status;
}

static int test_u8_tolower() {
  int status = TEST_OK;
  size_t i;
  int32_t actual;
  struct {
    int32_t input;
    int32_t expected;
  } tests[] = {
    {'\0', '\0'},
    {'A', 'a'},
    {'Z', 'z'},
    {'0', '0'},
    {L'Å', L'å'},
    {L'Ä', L'ä'},
    {L'Ö', L'ö'},
  };

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    actual = u8_tolower(tests[i].input);
    if (actual != tests[i].expected) {
      status = TEST_FAIL;
      TEST_LOGF("index:%zu expected:0x%08X actual:0x%08X", i,
          tests[i].expected, actual);
    }
  }

  return status;
}

TEST_ENTRY(
  {"u8_to_from_cp", test_u8_to_from_cp},
  {"u8_tolower", test_u8_tolower},
);
