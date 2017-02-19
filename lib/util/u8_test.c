#include <stdio.h>
#include <stdlib.h>
#include <lib/util/u8.h>

int main() {
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
    printf("%zu %d %d\n", nlen, vals[i].expected, actual);
	if (vals[i].expected != actual) {
      ret = EXIT_FAILURE;
	}
  }

  return ret;
}
