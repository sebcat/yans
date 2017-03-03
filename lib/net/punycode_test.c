#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/net/punycode.h>

int main() {
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

  for(i=0; vals[i].in != NULL; i++) {
    char *actual = punycode_encode(vals[i].in, strlen(vals[i].in));
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
    if (actual != NULL) {
      free(actual);
    }
  }
  return res;
}

