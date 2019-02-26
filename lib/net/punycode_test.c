#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/net/punycode.h>

static int test_encode() {
  int res = EXIT_SUCCESS;
  struct {
    char *in;
    char *expected;
  } vals[] = {
    {"", ""},
    {".", "."},
    {"..", ".."},
    {"...", "..."},
    {"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
    {"www.example.com", "www.example.com"},
    {"bücher.example.com", "xn--bcher-kva.example.com"},
    {NULL, NULL},
  };
  size_t i;
  int ret;
  struct punycode_ctx enc;

  ret = punycode_init(&enc);
  if (ret < 0) {
    perror("punycode_init");
    return EXIT_FAILURE;
  }

  for(i=0; vals[i].in != NULL; i++) {
    char *actual = punycode_encode(&enc, vals[i].in, strlen(vals[i].in));
    if (actual == NULL && vals[i].expected != NULL) {
      fprintf(stderr, "expected \"%s\", was NULL\n", vals[i].expected);
      res = EXIT_FAILURE;
    } else if (actual != NULL && vals[i].expected == NULL) {
      fprintf(stderr, "expected NULL, was \"%s\"\n", actual);
      res = EXIT_FAILURE;
    } else if (actual != NULL && vals[i].expected != NULL) {
      if (strcmp(actual, vals[i].expected) != 0) {
        fprintf(stderr, "expected \"%s\", was \"%s\"\n", vals[i].expected,
            actual);
        res = EXIT_FAILURE;
      }
    }
  }

  punycode_cleanup(&enc);
  return res;
}

int main() {
  size_t i;
  int res = EXIT_SUCCESS;
  struct {
    const char *name;
    int (*func)(void);
  } tests[] = {
    {"encode", test_encode},
    {NULL, NULL},
  };

  for (i = 0; tests[i].name != NULL; i++) {
    if (tests[i].func() == EXIT_SUCCESS) {
      fprintf(stderr, "OK  %s\n", tests[i].name);
    } else {
      fprintf(stderr, "ERR %s\n", tests[i].name);
      res = EXIT_FAILURE;
    }
  }

  return res;
}

