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
  char *start;
  char *dst;
  int num;

  if (!str) {
    return NULL;
  }

  start = dst = str;
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
  return start;
}

static void parse_pair(char *pair, char **key, char **val) {
  char *valptr;

  valptr = strchr(pair, '=');
  if (valptr) {
    *valptr = '\0';
    valptr++;
  }

  *key = urlquery_decode(pair);
  *val = urlquery_decode(valptr);
}

void urlquery_next_pair(char **str, char **key, char **val) {
  char *curr = *str;
  char *pair;

  /* skip leading pair separators, if any */
  curr += strspn(curr, "&");
  if (!*curr) {
    /* end of string reached: update str and return */
    *str = curr;
    return;
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
}
