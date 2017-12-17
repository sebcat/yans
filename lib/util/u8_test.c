#include <stdio.h>
#include <stdlib.h>
#include <lib/util/u8.h>

static int test_u8_to_cp() {
  struct {
    char *s;
    size_t len;
    int32_t expected;
  } vals[] = {
    {"", 1, 0},
    {"\x7f", 1, 0x7f},
    {"\xc2\x80", 2, 0x80},
    {"\xdf\xbf", 2, 0x07ff},
    {"\xe0\xa0\x80", 3, 0x0800},
    {"\xef\xbf\xbf", 3, 0xffff},
    {"\xf0\x90\x80\x80", 4, 0x010000},
    {"\xf4\x80\x83\xbf", 4, 0x1000ff},
    {NULL, 0, 0},
  };
  size_t i, nlen = 0xff;
  int32_t actual;
  int ret = EXIT_SUCCESS;

  for(i=0; vals[i].s != NULL; i++) {
    actual = u8_to_cp((uint8_t*)vals[i].s, vals[i].len, &nlen);
	if (vals[i].expected != actual) {
      ret = EXIT_FAILURE;
	}
  }

  return ret;
}

int main() {
  int ret = EXIT_SUCCESS;
  size_t i;
  struct {
    char *name;
    int (*func)(void);
  } tests[] = {
    {"u8_to_cp", test_u8_to_cp},
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
