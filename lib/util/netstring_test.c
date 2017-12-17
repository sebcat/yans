#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

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
  static struct {
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

    /* whitespace handling */
    {" \r\n\t 0",    {NULL}, {NETSTRING_ERRINCOMPLETE}},
    {" \r\n\t 0:",    {NULL}, {NETSTRING_ERRINCOMPLETE}},
    {" \r\n\t 0:,",    {"", NULL}, {NETSTRING_OK, NETSTRING_ERRINCOMPLETE}},
    {" \t\n\t 0:, 0:,", {"", "", NULL}, {NETSTRING_OK, NETSTRING_OK,
        NETSTRING_ERRINCOMPLETE}},
    {" \t\n\t 0:, 0:, \t\n\r ", {"", "", NULL}, {NETSTRING_OK, NETSTRING_OK,
        NETSTRING_ERRINCOMPLETE}},
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

#define MAX_TEST_NEXT 8

int test_next() {
  int ret;
  size_t i;
  size_t j;
  static struct {
    char *data;
    size_t noutputs;
    char *outputs[MAX_TEST_NEXT + 1];
  } inputs[] = {
    {
      .data = "",
      .noutputs = 0,
    },
    {
      .data = "3:foo,",
      .noutputs = 1,
      .outputs = {"foo", NULL},
    },
    {
      .data = "3:foo,3:bar,",
      .noutputs = 2,
      .outputs = {"foo", "bar", NULL},
    },
    {0},
  };

  for (i = 0; inputs[i].data != NULL; i++) {
    size_t tdatalen = strlen(inputs[i].data);
    char *tdata = strdup(inputs[i].data);
    char *curr = tdata;
    char *res;
    size_t reslen;
    for (j = 0; j < inputs[i].noutputs; j++) {
      ret = netstring_next(&res, &reslen, &curr, &tdatalen);
      if (ret != NETSTRING_OK) {
        fprintf(stderr, "input %zu, ns %zu: %s\n", i, j,
            netstring_strerror(ret));
        free(tdata);
        return EXIT_FAILURE;
      }

      if (strcmp(res, inputs[i].outputs[j]) != 0) {
        fprintf(stderr, "input:%zu ns:%zu expected:\"%s\" actual:\"%s\"\n",
            i, j, inputs[i].outputs[j], res);
        free(tdata);
        return EXIT_FAILURE;
      }
    }

    ret = netstring_next(&res, &reslen, &curr, &tdatalen);
    if (ret != NETSTRING_ERRINCOMPLETE) {
      fprintf(stderr, "input:%zu expected NETSTRING_ERRINCOMPLETE, got %d\n",
          i, ret);
      free(tdata);
      return EXIT_FAILURE;
    }

    free(tdata);
  }

  return EXIT_SUCCESS;
}

#define MAX_TEST_PAIRS 8

int test_next_pair() {
  size_t i, pi;
  int ret;
  struct netstring_pair actual = {0}, *expected;
  char *duped = NULL;
  char *data;
  size_t datalen;
  static struct {
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

    {"12:AAAAAAAAAA,13:BBBBBBBBBBB,14:CCCCCCCCCCCC,15:DDDDDDDDDDDDD,1:x,1:y,",
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

struct t {
  const char *foo;
  const char *bar;
  const char *baz;
  size_t foolen;
  size_t barlen;
  size_t bazlen;
};

struct netstring_map tm[] = {
  NETSTRING_MENTRY(struct t, foo),
  NETSTRING_MENTRY(struct t, bar),
  NETSTRING_MENTRY(struct t, baz),
  NETSTRING_MENTRY_END,
};

static bool cmpteq(size_t i, struct t *expected, struct t *actual) {

  if (expected->foo == NULL && actual->foo != NULL) {
    fprintf(stderr, "foo input: %zu expected NULL, was non-NULL\n", i);
    return false;
  } else if (expected->foo != NULL && actual->foo == NULL) {
    fprintf(stderr, "foo input: %zu expected NULL, was non-NULL\n", i);
    return false;
  } else if (expected->foo && actual->foo &&
      strcmp(expected->foo, actual->foo) != 0) {
    fprintf(stderr, "foo input: %zu expected \"%s\" was \"%s\"\n",
      i, expected->foo, actual->foo);
    return false;
  }

  if (expected->bar == NULL && actual->bar != NULL) {
    fprintf(stderr, "bar input: %zu expected NULL, was non-NULL\n", i);
    return false;
  } else if (expected->bar != NULL && actual->bar == NULL) {
    fprintf(stderr, "bar input: %zu expected NULL, was non-NULL\n", i);
    return false;
  } else if (expected->bar && actual->bar &&
      strcmp(expected->bar, actual->bar) != 0) {
    fprintf(stderr, "bar input: %zu expected \"%s\" was \"%s\"\n",
      i, expected->bar, actual->bar);
    return false;
  }

  if (expected->baz == NULL && actual->baz != NULL) {
    fprintf(stderr, "baz input: %zu expected NULL, was non-NULL\n", i);
    return false;
  } else if (expected->baz != NULL && actual->baz == NULL) {
    fprintf(stderr, "baz input: %zu expected NULL, was non-NULL\n", i);
    return false;
  } else if (expected->baz && actual->baz &&
      strcmp(expected->baz, actual->baz) != 0) {
    fprintf(stderr, "baz input: %zu expected \"%s\" was \"%s\"\n",
      i, expected->baz, actual->baz);
    return false;
  }

  return true;
}

int test_serialize() {
  size_t i;
  int ret;
  buf_t buf;
  static struct {
    struct t data;
    const char *expected;
  } inputs[] = {
    {
      {
        .foo = NULL,
        .bar = NULL,
        .baz = NULL,
        .foolen = 0,
        .barlen = 0,
        .bazlen = 0,
      },
      "0:,"
    },
    {
      {
        .foo = "",
        .bar = NULL,
        .baz = NULL,
        .foolen = 0,
        .barlen = 0,
        .bazlen = 0,
      },
      "0:,"
    },
    {
      {
        .foo = NULL,
        .bar = NULL,
        .baz = "",
        .foolen = 0,
        .barlen = 0,
        .bazlen = 0,
      },
      "0:,"
    },
    {
      {
        .foo = "",
        .bar = "",
        .baz = "",
        .foolen = 0,
        .barlen = 0,
        .bazlen = 0,
      },
      "0:,"
    },
    {
      {
        .foo = "bar",
        .bar = NULL,
        .baz = NULL,
        .foolen = 3,
        .barlen = 0,
        .bazlen = 0,
      },
      "14:3:foo,3:bar,,"
    },
    {
      {
        .foo = NULL,
        .bar = "bara",
        .baz = NULL,
        .foolen = 0,
        .barlen = 4,
        .bazlen = 0,
      },
      "15:3:bar,4:bara,,"
    },
    {
      {
        .foo = NULL,
        .bar = NULL,
        .baz = "bar",
        .foolen = 0,
        .barlen = 0,
        .bazlen = 3,
      },
      "14:3:baz,3:bar,,"
    },
    {
      {
        .foo = "a",
        .bar = "b",
        .baz = "c",
        .foolen = 1,
        .barlen = 1,
        .bazlen = 1,
      },
      "36:3:foo,1:a,3:bar,1:b,3:baz,1:c,,"
    },
    {{0}, NULL},
  };

  buf_init(&buf, 256);
  for (i = 0; inputs[i].expected != NULL; i++) {
    ret = netstring_serialize(&inputs[i].data, tm, &buf);
    if (ret != NETSTRING_OK) {
      fprintf(stderr, "input: %zu netstring_serialize failure: %s\n",
          i, netstring_strerror(ret));
      goto fail;
    }
    buf_achar(&buf, '\0');
    if (strcmp(buf.data, inputs[i].expected) != 0) {
      fprintf(stderr, "input: %zu expected:\"%s\" was:\"%s\"\n",
          i, inputs[i].expected, buf.data);
      goto fail;
    }
    buf_clear(&buf);
  }

  buf_cleanup(&buf);
  return EXIT_SUCCESS;
fail:
  buf_cleanup(&buf);
  return EXIT_FAILURE;
}

int test_deserialize() {
  size_t i;
  int ret;
  buf_t buf;
  struct t actual;
  size_t actual_left;
  static struct {
    const char *data;
    struct t expected;
    size_t expected_left;
  } inputs[] = {
    {
      "0:,",
      {
        .foo = NULL,
        .bar = NULL,
        .baz = NULL,
      },
      0,
    },
    {
      "0:,z",
      {
        .foo = NULL,
        .bar = NULL,
        .baz = NULL,
      },
      1,
    },
    {
      "11:3:foo,0:,,",
      {
        .foo = NULL,
        .bar = NULL,
        .baz = NULL,
      },
      0,
    },
    {
      "14:3:foo,3:bar,,",
      {
        .foo = "bar",
        .bar = NULL,
        .baz = NULL,
      },
      0,
    },
    {
      "14:3:bar,3:bar,,",
      {
        .foo = NULL,
        .bar = "bar",
        .baz = NULL,
      },
      0,
    },
    {
      "14:3:baz,3:bar,,",
      {
        .foo = NULL,
        .bar = NULL,
        .baz = "bar",
      },
      0,
    },
    {
      "36:3:foo,1:a,3:bar,1:b,3:baz,1:c,,",
      {
        .foo = "a",
        .bar = "b",
        .baz = "c",
      },
      0,
    },
    {
      "36:3:foo,1:a,3:bar,1:b,3:baz,1:c,,topkek",
      {
        .foo = "a",
        .bar = "b",
        .baz = "c",
      },
      6,
    },
    {NULL, {0}},
  };

  buf_init(&buf, 256);
  for (i = 0; inputs[i].data != NULL; i++) {
    buf_adata(&buf, inputs[i].data, strlen(inputs[i].data));
    memset(&actual, 0, sizeof(actual));
    ret = netstring_deserialize(&actual, tm, buf.data, buf.len, &actual_left);
    if (ret != NETSTRING_OK) {
      fprintf(stderr, "input: %zu netstring_deserialize failure: %s\n",
          i, netstring_strerror(ret));
      goto fail;
    }
    if (!cmpteq(i, &inputs[i].expected, &actual)) {
      goto fail;
    }
    if (inputs[i].expected_left != actual_left) {
      fprintf(stderr, "input: %zu expected_left: %zu actual_left: %zu\n",
          i, inputs[i].expected_left, actual_left);
      goto fail;
    }
    buf_clear(&buf);
  }

  buf_cleanup(&buf);
  return EXIT_SUCCESS;
fail:
  buf_cleanup(&buf);
  return EXIT_FAILURE;
}

int main() {
  size_t i;
  int ret = EXIT_SUCCESS;
  static struct {
    char *name;
    int (*callback)(void);
  } tests[] = {
    {"parse", test_parse},
    {"next", test_next},
    {"next_pair", test_next_pair},
    {"serialize", test_serialize},
    {"deserialize", test_deserialize},
    {NULL, NULL},
  };

  for (i = 0; tests[i].name != NULL; i++) {
    if (tests[i].callback() == EXIT_SUCCESS) {
      fprintf(stderr, "OK  %s\n", tests[i].name);
    } else {
      fprintf(stderr, "ERR %s\n", tests[i].name);
      ret = EXIT_FAILURE;
    }
  }

  return ret;
}
