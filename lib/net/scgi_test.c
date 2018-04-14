#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/net/scgi.h>

#define MAX_TEST_HEADERS 8

static int cmprest(const char *expected, const char *actual, size_t num) {

  if (expected == NULL && actual == NULL) {
    return EXIT_SUCCESS;
  } else if (expected == NULL && actual != NULL) {
    fprintf(stderr, "%zu: expected == NULL, actual == \"%s\"\n",
        num, actual);
    return EXIT_FAILURE;
  } else if (expected != NULL && actual == NULL) {
    fprintf(stderr, "%zu: expected == \"%s\", actual == NULL\n",
        num, expected);
    return EXIT_FAILURE;
  } else if (strcmp(expected, actual) != 0) {
    fprintf(stderr, "%zu: expected == \"%s\", actual == \"%s\"\n",
        num, expected, actual);
    return EXIT_FAILURE;
  } else {
    return EXIT_SUCCESS;
  }
}

static int cmphdrs(struct scgi_ctx *ctx, struct scgi_header *expected,
    size_t num) {
  size_t i;
  int ret;
  struct scgi_header hdr = {0};
  int status = EXIT_SUCCESS;

  for (i = 0; i < MAX_TEST_HEADERS; i++) {
    if (expected[i].keylen == 0) {
      break;
    }

    ret = scgi_get_next_header(ctx, &hdr);
    if (ret != SCGI_AGAIN) {
      fprintf(stderr, "%zu,%zu: expected SCGI_AGAIN, got: %s\n", num, i,
          scgi_strerror(ret));
      status = EXIT_FAILURE;
      continue;
    }

    /* maybe do NULL-/size-checks on actual here, but yeah... mañana */

    if (expected[i].keylen != hdr.keylen) {
      fprintf(stderr, "%zu,%zu: expected keylen != actual keylen (%zu, %zu)\n",
          num, i, expected[i].keylen, hdr.keylen);
      status = EXIT_FAILURE;
      continue;
    }

    if (expected[i].valuelen != hdr.valuelen) {
      fprintf(stderr, "%zu,%zu: unexpected valuelen (%zu, %zu)\n",
          num, i, expected[i].valuelen, hdr.valuelen);
      status = EXIT_FAILURE;
      continue;
    }

    if (strcmp(expected[i].key, hdr.key) != 0) {
      fprintf(stderr, "%zu,%zu: unexpected key (%s, %s)\n",
          num, i, expected[i].key, hdr.key);
      status = EXIT_FAILURE;
      continue;
    }

    if (strcmp(expected[i].value, hdr.value) != 0) {
      fprintf(stderr, "%zu,%zu: unexpected value (%s, %s)\n",
          num, i, expected[i].value, hdr.value);
      status = EXIT_FAILURE;
      continue;
    }
  }

  /* make sure we get SCGI_OK on the last one */
  ret = scgi_get_next_header(ctx, &hdr);
  if (ret != SCGI_OK) {
    fprintf(stderr,
        "%zu: expected SCGI_OK, got: %s\n", num, scgi_strerror(ret));
    status = EXIT_FAILURE;
  }

  return status;
}


static int test_parse_ok() {
  int res = EXIT_SUCCESS;
  struct {
    const char *in;
    size_t inlen;
    struct scgi_header expected_headers[MAX_TEST_HEADERS];
    const char *expected_body;
  } vals[] = {
    {
      .in = "0:,",
      .inlen = 3,
      .expected_headers = {{0}},
      .expected_body = NULL
    },
    {
      .in = "0:,foobar",
      .inlen = 9,
      .expected_headers = {{0}},
      .expected_body = "foobar",
    },
    {
      .in = "8:FOO\0BAR\0,",
      .inlen = 11,
      .expected_headers = {
        {
          .keylen = 3,
          .key = "FOO",
          .valuelen = 3,
          .value = "BAR",
        },
      },
      .expected_body = NULL
    },
    {
      .in = "8:FXO\0BXR\0,foobar",
      .inlen = 17,
      .expected_headers = {
        {
          .keylen = 3,
          .key = "FXO",
          .valuelen = 3,
          .value = "BXR",
        },
      },
      .expected_body = "foobar"},

    {0},
  };
  size_t i;

  for(i=0; vals[i].in != NULL; i++) {
    struct scgi_ctx ctx;
    int ret;

    scgi_init(&ctx, -1, SCGI_DEFAULT_MAXHDRSZ);
    scgi_adata(&ctx, vals[i].in, vals[i].inlen);
    ret = scgi_parse_header(&ctx);
    if (ret != SCGI_OK) {
      fprintf(stderr, "%zu: header parse failure: %s\n", i, scgi_strerror(ret));
      res = EXIT_FAILURE;
      continue;
    }
    if (cmphdrs(&ctx, vals[i].expected_headers, i) != EXIT_SUCCESS) {
      res = EXIT_FAILURE;
    }

    if (cmprest(vals[i].expected_body, scgi_get_rest(&ctx, NULL), i) !=
        EXIT_SUCCESS) {
      res = EXIT_FAILURE;
    }

    scgi_cleanup(&ctx);
  }

  return res;
}

int main() {
  size_t i;
  int res = EXIT_SUCCESS;
  struct {
    const char *name;
    int (*func)(void);
  } tests[] = {
    {"parse_ok", test_parse_ok},
    /* TODO: parse_fail, &c */
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

