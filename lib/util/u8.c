#include <stdio.h>

#include <lib/util/u8.h>

#define UNICODE_REPLACEMENT_CHAR 0xFFFD
/*
http://www.cprogramming.com/tutorial/unicode.html
00000000 -- 0000007F: 	0xxxxxxx
00000080 -- 000007FF: 	110xxxxx 10xxxxxx
00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

candidate for clean up, w/ added validation
*/
int32_t u8_to_cp(const uint8_t *s, size_t len, size_t *width) {
  if (len >= 1 && s[0] <= 0x7f) {
    if (width != NULL) {
      *width = 1;
    }
    return (int32_t)s[0];
  } else if (len >= 2 &&
      (s[0] & 0xe0) == 0xc0 &&
      (s[1] & 0xc0) == 0x80) {
    if (width != NULL) {
      *width = 2;
    }
    return (int32_t)((((uint32_t)s[0] & 0x1f) << 6) | (s[1] & 0x3f));
  } else if (len >= 3 &&
      (s[0] & 0xf0) == 0xe0 &&
      (s[1] & 0xc0) == 0x80 &&
      (s[2] & 0xc0) == 0x80) {
    if (width != NULL) {
      *width = 3;
    }
    return (int32_t)(
        (((uint32_t)s[0] & 0x0f) << 12) |
        (((uint32_t)s[1] & 0x3f) << 6) |
        ((uint32_t)s[2] & 0x3f));
  } else if (len >= 4 &&
      (s[0] & 0xf8) == 0xf0 &&
      (s[1] & 0xc0) == 0x80 &&
      (s[2] & 0xc0) == 0x80 &&
      (s[3] & 0xc0) == 0x80) {
    if (width != NULL) {
      *width = 4;
    }
    return (int32_t)(
        (((uint32_t)s[0] & 0x07) << 18) |
        (((uint32_t)s[1] & 0x3f) << 12) |
        (((uint32_t)s[2] & 0x3f) << 6) |
        ((uint32_t)s[3] & 0x3f));
  } else {
    if (width != NULL) {
      *width = 1;
    }
    return UNICODE_REPLACEMENT_CHAR;
  }
}

#ifdef WOLOLO
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

  for(i=0; vals[i].s != NULL; i++) {
    actual = u8_to_cp((uint8_t*)vals[i].s, vals[i].len, &nlen);
    printf("%zu %d %d\n", nlen, vals[i].expected, actual);
  }

  return 0;
}
#endif
