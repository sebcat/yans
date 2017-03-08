#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lib/util/netstring.h>

#define MAX_EXPECTED 16

int main() {
  size_t i;
  char *buf;
  size_t buflen;
  char *res;
  char *curr;
  size_t reslen;
  size_t j;
  int ret;
  int status = EXIT_SUCCESS;
  struct {
    const char *in;
    char *expected_strs[MAX_EXPECTED];
    int expected_rets[MAX_EXPECTED];
  } inputs[] = {
    {"",       {NULL}, {NS_ERRINCOMPLETE}},
    {":",      {NULL}, {NS_ERRFMT} },
    {"0",      {NULL}, {NS_ERRINCOMPLETE}},
    {"0:",     {NULL}, {NS_ERRINCOMPLETE}},

    {"0:,",    {"", NULL}, {NS_OK, NS_ERRINCOMPLETE}},
    {"0:,0",   {"", NULL}, {NS_OK, NS_ERRINCOMPLETE}},
    {"0:,0:",  {"", NULL}, {NS_OK, NS_ERRINCOMPLETE}},
    {"0:,0:,", {"", "", NULL}, {NS_OK, NS_OK, NS_ERRINCOMPLETE}},

    {"1:a,1:b,", {"a", "b", NULL}, {NS_OK, NS_OK, NS_ERRINCOMPLETE}},
    {"4:sven,3:ior,", {"sven", "ior", NULL}, {NS_OK, NS_OK, NS_ERRINCOMPLETE}},
    {"4:sven,3:ior,wiie", {"sven", "ior", NULL}, {NS_OK, NS_OK, NS_ERRFMT}},
    {NULL, {0}, {0}},
  };

  for (i = 0; inputs[i].in != NULL; i++) {
    buflen = strlen(inputs[i].in);
    curr = buf = strdup(inputs[i].in);
    j = 0;
    do {
      ret = netstring_parse(&res, &reslen, curr, strlen(curr));
      if (ret != inputs[i].expected_rets[j]) {
        fprintf(stderr, "input (%zu, %zu): expected ret \"%s\", was \"%s\"\n",
            i, j,
            netstring_strerror(inputs[i].expected_rets[j]),
            netstring_strerror(ret));
        status = EXIT_FAILURE;
      }
      if (ret == NS_OK) {
        if (strcmp(res, inputs[i].expected_strs[j]) != 0) {
          fprintf(stderr, "input (%zu, %zu): expected \"%s\", was \"%s\"\n",
              i, j, inputs[i].expected_strs[j], res);
          status = EXIT_FAILURE;
        }
        curr = res + reslen + 1;
      }
      j++;
    } while(ret == NS_OK && curr < (buf+buflen));
    free(buf);
  }
  return status;
}
