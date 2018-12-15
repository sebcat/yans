#ifndef UTIL_TEST_H__
#define UTIL_TEST_H__
#include <stdlib.h>
#include <stdio.h> 

#define TEST_OK 42
#define TEST_FAIL -1

/*NB: str,fmt must be a string literal */
#define TEST_LOG_ERR(str) \
    fprintf(stderr, "  %s:%d %s\n", __FILE__, __LINE__, str)
#define TEST_LOG_ERRF(fmt, ...) \
    fprintf(stderr, "  %s:%d " fmt "\n", __FILE__, __LINE__, __VA_ARGS__)

#define TEST_ENTRY(...) \
int main() { \
  int ret = EXIT_SUCCESS; \
  size_t i; \
  struct { \
    char *name; \
    int (*func)(void); \
  } tests[] = { \
    __VA_ARGS__ \
    {NULL, NULL}, \
  }; \
  for (i = 0; tests[i].name != NULL; i++) { \
    if (tests[i].func() == TEST_OK) { \
      fprintf(stderr, "OK  %s\n", tests[i].name); \
    } else { \
      fprintf(stderr, "ERR %s\n", tests[i].name); \
      ret = EXIT_FAILURE; \
    } \
  } \
  return ret; \
}

#endif
