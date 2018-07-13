#include <stdbool.h>
#include <string.h>

#include <lib/util/str.h>

static const char *in_set(const char *s, size_t len, char ch) {
  size_t i;

  for (i = 0; i < len; i++) {
    if (s[i] == ch) {
      return s + i;
    }
  }

  return NULL;
}

int str_map_field(const char *s, size_t len,
    const char *seps, size_t sepslen,
    int (*func)(const char *, size_t, void *),
    void *data) {
  size_t i;
  size_t start = 0;
  bool in_str = false;
  int ret;
  char ch;

  /* two-state/boolean FSM with the current character as input */
  for (i = 0; i < len; i++) {
    ch = s[i];
    if (in_set(seps, sepslen, ch)) {
      if (in_str) {
        ret = func(s + start, i - start, data);
        if (ret <= 0) {
          return ret;
        }
        in_str = !in_str;
      }
    } else {
      if (!in_str) {
        start = i;
        in_str = !in_str;
      }
    }
  }

  /* handle trailing field w/ no trailing field separator */
  if (in_str && start < i) {
    return func(s + start, i - start, data);
  }

  return 0;
}

int str_map_fieldz(const char *s, const char *seps,
    int (*func)(const char *, size_t, void *),
    void *data) {
  size_t i;
  size_t start = 0;
  bool in_str = false;
  int ret;
  char ch;

  /* two-state/boolean FSM with the current character as input */
  for (i = 0; (ch = s[i]) != '\0'; i++) {
    if (strchr(seps, ch)) {
      if (in_str) {
        ret = func(s + start, i - start, data);
        if (ret <= 0) {
          return ret;
        }
        in_str = !in_str;
      }
    } else {
      if (!in_str) {
        start = i;
        in_str = !in_str;
      }
    }
  }

  /* handle trailing field w/ no trailing field separator */
  if (in_str && start < i) {
    return func(s + start, i - start, data);
  }

  return 0;
}

