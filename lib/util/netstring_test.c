#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lib/util/netstring.h>

#define MAX_EXPECTED 16

int test_parse() {
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
    {"",       {NULL}, {NETSTRING_ERRINCOMPLETE}},
    {":",      {NULL}, {NETSTRING_ERRFMT} },
    {"0",      {NULL}, {NETSTRING_ERRINCOMPLETE}},
    {"0:",     {NULL}, {NETSTRING_ERRINCOMPLETE}},

    {"0:,",    {"", NULL}, {NETSTRING_OK, NETSTRING_ERRINCOMPLETE}},
    {"0:,0",   {"", NULL}, {NETSTRING_OK, NETSTRING_ERRINCOMPLETE}},
    {"0:,0:",  {"", NULL}, {NETSTRING_OK, NETSTRING_ERRINCOMPLETE}},
    {"0:,0:,", {"", "", NULL}, {NETSTRING_OK, NETSTRING_OK,
        NETSTRING_ERRINCOMPLETE}},

    {"1:a,1:b,", {"a", "b", NULL}, {NETSTRING_OK, NETSTRING_OK,
        NETSTRING_ERRINCOMPLETE}},
    {"4:sven,3:ior,", {"sven", "ior", NULL}, {NETSTRING_OK, NETSTRING_OK,
        NETSTRING_ERRINCOMPLETE}},
    {"4:sven,3:ior,wiie", {"sven", "ior", NULL}, {NETSTRING_OK, NETSTRING_OK,
        NETSTRING_ERRFMT}},
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
      if (ret == NETSTRING_OK) {
        if (strcmp(res, inputs[i].expected_strs[j]) != 0) {
          fprintf(stderr, "input (%zu, %zu): expected \"%s\", was \"%s\"\n",
              i, j, inputs[i].expected_strs[j], res);
          status = EXIT_FAILURE;
        }
        curr = res + reslen + 1;
      }
      j++;
    } while(ret == NETSTRING_OK && curr < (buf+buflen));
    free(buf);
  }
  return status;
}

#define MAX_TEST_PAIRS 8

int test_next_pair() {
  size_t i, pi;
  int ret;
  struct netstring_pair actual = {0}, *expected;
  char *duped = NULL;
  char *data;
  size_t datalen;
  struct {
    char *data;
    size_t npairs;
    struct netstring_pair pairs[MAX_TEST_PAIRS];
  } inputs[] = {
    {"", 0, {{0}}},
    {"3", 0, {{0}}},
    {"3:", 0, {{0}}},
    {"3:f", 0, {{0}}},
    {"3:fo", 0, {{0}}},
    {"3:foo", 0, {{0}}},
    {"3:foa,", 0, {{0}}},
    {"3:fob,3:bar", 0, {{0}}},

    {"3:foc,3:bar,", 1, {
      {"foc", 3, "bar", 3},
    }},

    {"4:xooo,2:xa,3:oof,6:rabies,", 2, {
      {"xooo", 4, "xa", 2},
      {"oof", 3, "rabies", 6},
    }},

    {"10:AAAAAAAAAA,11:BBBBBBBBBBB,12:CCCCCCCCCCCC,13:DDDDDDDDDDDDD,1:x,1:y,",
      3, {
        {"AAAAAAAAAA", 10, "BBBBBBBBBBB", 11},
        {"CCCCCCCCCCCC", 12, "DDDDDDDDDDDDD", 13},
        {"x", 1, "y", 1},
    }},

    {NULL, 0},
  };

  for (i = 0; inputs[i].data != NULL; i++) {
    duped = strdup(inputs[i].data);
    data = duped;
    datalen = strlen(inputs[i].data);

    /* parse all expected pairs */
    for (pi = 0; pi < inputs[i].npairs; pi++) {
      expected = &inputs[i].pairs[pi];
      ret = netstring_next_pair(&actual, &data, &datalen);
      if (ret != NETSTRING_OK) {
        fprintf(stderr, "input %zu, pair %zu: %s\n", i, pi,
            netstring_strerror(ret));
        goto fail;
      }

      /* check pair length */
      if (actual.keylen != expected->keylen) {
        fprintf(stderr, "input %zu, pair %zu: "
            "keylen actual: %zu expected: %zu\n", i, pi, actual.keylen,
            expected->keylen);
        goto fail;
      } else if (actual.valuelen != expected->valuelen) {
        fprintf(stderr, "input %zu, pair %zu: "
            "valuelen actual: %zu expected: %zu\n", i, pi, actual.valuelen,
            expected->valuelen);
        goto fail;
      }

      /* check pair values */
      if (memcmp(actual.key, expected->key, expected->keylen) != 0) {
        fprintf(stderr, "input %zu, pair: %zu, actual key: \"%s\" "
            "expected key: \"%s\"\n", i, pi, actual.key, expected->key);
        goto fail;
      } else if (memcmp(actual.value, expected->value, expected->valuelen)
          != 0) {
        fprintf(stderr, "input %zu, pair: %zu, actual value: \"%s\" "
            "expected value: \"%s\"\n", i, pi, actual.value, expected->value);
        goto fail;
      }
    }

    ret = netstring_next_pair(&actual, &data, &datalen);
    if (ret != NETSTRING_ERRINCOMPLETE) {
      fprintf(stderr, "end parsing: input %zu, pair %zu: %s\n"
          "data: \"%*s\"\n", i, pi, netstring_strerror(ret), (int)datalen,
          data);
      goto fail;
    }
    free(duped);
    duped = NULL;
  }
  return EXIT_SUCCESS;
fail:
  if (duped != NULL) {
    free(duped);
  }
  return EXIT_FAILURE;
}

int main() {
  size_t i;
  struct {
    char *name;
    int (*callback)(void);
  } tests[] = {
    {"parse", test_parse},
    {"next_pair", test_next_pair},
    {NULL, NULL},
  };

  for (i = 0; tests[i].name != NULL; i++) {
    if (tests[i].callback() != EXIT_SUCCESS) {
      fprintf(stderr, "FAIL: %s (%zu)\n", tests[i].name, i);
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
