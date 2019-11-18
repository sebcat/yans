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
#include <ctype.h>

#include <lib/net/urlquery.h>

static int fromhex(int ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  } else if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  } else {
    return 0;
  }
}

char *urlquery_decode(char *str) {
  char *dst;
  int num;

  if (!str) {
    return NULL;
  }

  dst = str;
  while(*str) {
    if (str[0] == '%' && isxdigit(str[1]) && isxdigit(str[2])) {
      num = fromhex(str[1]) * 16 + fromhex(str[2]);
      *dst = num;
      str += 2;
    } else if (str[0] == '+') {
      *dst = ' ';
    } else {
      *dst = str[0];
    }

    dst++;
    str++;
  }

  *dst = '\0';
  return dst; /* allows us to calculate length of decoded string */
}

static void parse_pair(char *pair, char **key, char **val) {
  char *valptr;
  char *tmp;

  valptr = strchr(pair, '=');
  if (valptr) {
    *valptr = '\0';
    valptr++;
  }

  tmp = urlquery_decode(pair);
  if (valptr) {
    urlquery_decode(valptr);
    tmp = valptr;
  }

  *key = pair;
  *val = tmp;
}

int urlquery_next_pair(char **str, char **key, char **val) {
  char *curr;
  char *pair;

  *key = NULL;
  *val = NULL;
  if (!str || !*str) {
    return 0;
  }

  curr = *str;

  /* skip leading pair separators, if any */
  curr += strspn(curr, "&");
  if (!*curr) {
    /* end of string reached: update str and return */
    *str = curr;
    return 0;
  }

  /* at this point, we have a non-empty string pointed to by pair. We find
   * the next pair separator or end of string. If we find a separator we
   * terminate it and advance to the next character */
  pair = curr;
  curr += strcspn(curr, "&");
  if (*curr) {
    *curr = '\0';
    curr++;
  }

  parse_pair(pair, key, val);
  *str = curr;
  return 1;
}
