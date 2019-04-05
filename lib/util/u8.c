#include <stdio.h>
#include <wctype.h>

#include <lib/util/u8.h>

#define UNICODE_REPLACEMENT_CHAR       0xFFFD
#define UNICODE_REPLACEMENT_CHAR_BYTES 2
/*
http://www.cprogramming.com/tutorial/unicode.html
00000000 -- 0000007F: 	0xxxxxxx
00000080 -- 000007FF: 	110xxxxx 10xxxxxx
00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/
int32_t u8_to_cp(const char *str, size_t len, size_t *width) {
  const uint8_t *s = (const uint8_t*)str;
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

/* return -1 if 'len' is too small, 0 otherwise */
int u8_from_cp(char *s, size_t len, int32_t cp, size_t *width) {
again:
  if (cp >= 0 && cp < 0x80 && len >= 1) {
    s[0] = (uint8_t)cp;
    if (width != NULL) {
      *width = 1;
    }
  } else if (cp < 0x800 && len >= 2) {
    /* 110xxxxx 10xxxxxx -> 00000080 -- 000007FF */
    /* 11111 111111*/
    s[0] = (uint32_t)cp >> 6 | 0xc0;
    s[1] = ((uint32_t)cp & 0x3f) | 0x80;
    if (width != NULL) {
      *width = 2;
    }
  } else if (cp < 0x10000 && len >= 3) {
    /* 1110xxxx 10xxxxxx 10xxxxxx: 00000800 -- 0000FFFF */
    s[0] = ((uint32_t)cp >> 12) | 0xe0;
    s[1] = (((uint32_t)cp >> 6) & 0x3f) | 0x80;
    s[2] = ((uint32_t)cp & 0x3f) | 0x80;
    if (width != NULL) {
      *width = 3;
    }
  } else if (cp < 0x200000 && len >= 4) {
    /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx: 00010000 -- 001FFFFF */
    s[0] = ((uint32_t)cp >> 18) | 0xf0;
    s[1] = (((uint32_t)cp >> 12) & 0x3f) | 0x80;
    s[2] = (((uint32_t)cp >> 6) & 0x3f) | 0x80;
    s[3] = ((uint32_t)cp & 0x3f) | 0x80;
    if (width != NULL) {
      *width = 4;
    }
  } else if (len >= UNICODE_REPLACEMENT_CHAR_BYTES) {
    cp = UNICODE_REPLACEMENT_CHAR;
    goto again;
  } else {
    if (width != NULL) {
      *width = 0;
    }
    return -1;
  }

  return 0;
}

int32_t u8_tolower(int32_t cp) {
  return (int32_t)towlower((wint_t)cp);
}
