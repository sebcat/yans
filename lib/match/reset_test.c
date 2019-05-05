#include <string.h>
#include <errno.h>

#include <lib/match/reset.h>
#include <lib/match/reset_csv.h>
#include <lib/util/macros.h>
#include <lib/util/test.h>

#define HTTPHEADER_PATTERN_FILE "./data/pm/1.pm"
#define HTTPHEADER_MIN_ROWS 18 /* subject to change, update as needed */

int test_match() {
  int status = TEST_OK;
  static const struct {
    const char *re;
    const char *data;
    size_t len;
  } inputs[] = {
    {"", ""},
    {"", "trololo"},
    {"foo", "foo"},
    {"bar", "foo\0bar", sizeof("foo\0bar")-1},
  };
  size_t i;
  int ret;
  int id;
  int found;
  reset_t *reset;

  reset = reset_new();
  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    ret = reset_add_pattern(reset, inputs[i].re);
    if (ret != (int)i) {
      status = TEST_FAIL;
      TEST_LOGF("reset_add_pattern ID mismatch i:%zu %s", i, reset_strerror(reset));
    }
  }

  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("%s", reset_strerror(reset));
  }

  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    ret = reset_match(reset, inputs[i].data,
        inputs[i].len == 0 ? strlen(inputs[i].data) : inputs[i].len);
    if (ret != RESET_OK) {
      status = TEST_FAIL;
      TEST_LOGF("i:%zu %s", i, reset_strerror(reset));
    }

    found = 0;
    while ((id = reset_get_next_match(reset)) >= 0) {
      if (id == (int)i) {
        found = 1;
        break;
      }
    }

    if (!found) {
      status = TEST_FAIL;
      TEST_LOGF("i:%zu mismatch", i);
    }
  }

  reset_free(reset);
  return status;
}

int test_noadd() {
  int status = TEST_OK;
  static const struct {
    const char *re;
  } inputs[] = {
    {"foo["},
    {"foo\\"},
    {"foo("},
  };
  size_t i;
  int ret;
  reset_t *reset;

  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    reset = reset_new();
    ret = reset_add_pattern(reset, inputs[i].re);
    if (ret == RESET_ERR) {
      TEST_LOGF("expected failure i:%zu %s", i, reset_strerror(reset));
    } else {
      TEST_LOGF("unexpected success i:%zu", i);
      status = TEST_FAIL;
    }

    reset_free(reset);
  }

  return status;
}

int test_nomatch() {
  int status = TEST_OK;
  static const struct {
    const char *re;
    const char *data;
    size_t len;
  } inputs[] = {
    {"foo", ""},
    {"foo", "bar"},
    {"baz", "foo\0bar", sizeof("foo\0bar")-1},
  };
  size_t i;
  int ret;
  int id;
  int found;
  reset_t *reset;

  reset = reset_new();
  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    ret = reset_add_pattern(reset, inputs[i].re);
    if (ret != (int)i) {
      status = TEST_FAIL;
      TEST_LOGF("reset_add_pattern ID mismatch i:%zu %s", i, reset_strerror(reset));
    }
  }

  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("%s", reset_strerror(reset));
  }

  for (i = 0; i < ARRAY_SIZE(inputs); i++) {
    reset_match(reset, inputs[i].data,
        inputs[i].len == 0 ? strlen(inputs[i].data) : inputs[i].len);
    found = 0;
    while ((id = reset_get_next_match(reset)) >= 0) {
      if (id == (int)i) {
        found = 1;
        break;
      }
    }

    if (found) {
      status = TEST_FAIL;
      TEST_LOGF("i:%zu unexpected match", i);
    }
  }

  reset_free(reset);
  return status;
}

int test_substrings() {
  int status = TEST_OK;
  reset_t *reset;
  int ret;
  int id;
  const char data[] = "foo adam bar bertil baz cesar";
  const char *sub;

  reset = reset_new();
  reset_add_pattern(reset, "foo ([a-z]+)"); /* 0 */
  reset_add_pattern(reset, "bar ([a-z]+)"); /* 1 */
  reset_add_pattern(reset, "baz ([a-z]+)"); /* 2 */
  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("%s", reset_strerror(reset));
  }

  reset_match(reset, data, sizeof(data)-1);
  while ((id = reset_get_next_match(reset)) >= 0) {
    sub = reset_get_substring(reset, id, data, sizeof(data)-1, NULL);
    if (sub == NULL) {
      status = TEST_FAIL;
      TEST_LOGF("missing substring for id %d", id);
      continue;
    }

    switch (id) {
    case 0:
      if (strcmp(sub, "adam") != 0) {
        status = TEST_FAIL;
        TEST_LOGF("expected adam, got %s", sub);
      }
      break;
    case 1:
      if (strcmp(sub, "bertil") != 0) {
        status = TEST_FAIL;
        TEST_LOGF("expected bertil, got %s", sub);
      }
      break;
    case 2:
      if (strcmp(sub, "cesar") != 0) {
        status = TEST_FAIL;
        TEST_LOGF("expected cesar, got %s", sub);
      }
      break;
    default:
      status = TEST_FAIL;
      TEST_LOGF("unexpected id: %d", id);
    }
  }

  reset_free(reset);
  return status;
}

static int test_match_httpheaders() {
  int ret;
  int id;
  size_t i;
  int status = TEST_OK;
  size_t npatterns;
  FILE *fp;
  reset_t *reset;
  struct {
    const char *input;
    enum reset_match_type type;
    const char *name;
    const char *version;
  } tests[] = {
    {
      .input = "HTTP/2 301 \r\nserver: nginx\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "nginx/nginx"
    },
    {
      .input = "HTTP/2 301 \r\nServer: nginx/1.14.2\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "nginx/nginx",
      .version = "1.14.2"
    },
    {
      .input = "HTTP/2 301 \r\nx-generator: drupal\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "drupal/drupal"
    },
    {
      .input = "HTTP/2 301 \r\nX-Generator: Drupal 8 (https://www.drupal.org)\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "drupal/drupal",
      .version = "8"
    },
    {
      .input = "HTTP/2 301 \r\nx-drupal-cache: HIT\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "drupal/drupal"
    },
    {
      .input = "HTTP/2 301 \r\nx-magento-cloud-cache: MISS\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "magento/magento"
    },
    {
      .input = "HTTP/2 301 \r\nx-powered-by: ASP.NET\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "microsoft/asp.net"
    },
    {
      .input = "HTTP/2 301 \r\nx-powered-by: EasyEngine\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "easyengine/easyengine"
    },
    {
      .input = "HTTP/2 301 \r\nX-Powered-By: EasyEngine 3.8.1\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "easyengine/easyengine",
      .version = "3.8.1"
    },
    {
      .input = "HTTP/2 301 \r\nserver: litespeed\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "litespeed/litespeed"
    },
    {
      .input = "HTTP/2 301 \r\nx-litespeed-cache: hit\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "litespeed/litespeed"
    },
    {
      .input = "HTTP/2 301 \r\nx-powered-by: PHP\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "php/php"
    },
    {
      .input = "HTTP/2 301 \r\nX-Powered-By: PHP/7.2.5\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "php/php",
      .version = "7.2.5"
    },
    {
      .input = "HTTP/2 301 \r\nServer: awselb\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "amazon/awselb"
    },
    {
      .input = "HTTP/2 301 \r\nserver: awselb/2.0\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "amazon/awselb",
      .version = "2.0"
    },
    {
      .input = "HTTP/2 301 \r\nServer: AmazonS3\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "amazon/s3"
    },
    {
      .input = "HTTP/2 301 \r\nServer: cloudflare\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "cloudflare/cloudflare"
    },
    {
      .input = "HTTP/2 301 \r\nserver: cloudflare\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "cloudflare/cloudflare"
    },
    {
      .input = "HTTP/2 301 \r\nServer: cloudfront\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "cloudflare/cloudfront"
    },
    {
      .input = "HTTP/2 301 \r\nserver: CloudFront\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "cloudflare/cloudfront"
    },
    {
      .input = "HTTP/2 301 \r\nserver: apache",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/apache"
    },
    {
      .input = "HTTP/2 301 \r\nServer: Apache/2.4.6 (CentOS) OpenSSL/1.0.2k-fips",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/apache",
      .version = "2.4.6"
    },
    {
      .input = "HTTP/2 301 \r\nServer: Apache/2.4.6 (CentOS) OpenSSL/1.0.2k-fips",
      .type = RESET_MATCH_COMPONENT,
      .name = "redhat/centos"
    },
    {
      .input = "HTTP/2 301 \r\nServer: Apache/2.4.6 (Debian) OpenSSL/1.0.2k-fips",
      .type = RESET_MATCH_COMPONENT,
      .name = "debian/debian"
    },
    {
      .input = "HTTP/2 301 \r\nServer: Apache/2.4.6 (Ubuntu) OpenSSL/1.0.2k-fips",
      .type = RESET_MATCH_COMPONENT,
      .name = "canonical/ubuntu"
    },
    {
      .input = "HTTP/2 301 \r\nServer: Apache/2.4.6 (Ubuntu) OpenSSL/1.0.2k-fips",
      .type = RESET_MATCH_COMPONENT,
      .name = "openssl/openssl",
      .version = "1.0.2k-fips"
    },
    {
      .input = "HTTP/2 301 \r\nServer: Apache/2.4.6 (Ubuntu) OpenSSL",
      .type = RESET_MATCH_COMPONENT,
      .name = "openssl/openssl"
    },
  };

  reset = reset_new();
  if (reset == NULL) {
    TEST_LOG("reset_new failure");
    return TEST_FAIL;
  }

  fp = fopen(HTTPHEADER_PATTERN_FILE, "rb");
  if (!fp) {
    status = TEST_FAIL;
    TEST_LOGF(HTTPHEADER_PATTERN_FILE ": %s", strerror(errno));
    goto done;
  }

  ret = reset_csv_load(reset, fp, &npatterns);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("failed to load patterns from CSV: %s",
        reset_strerror(reset));
    goto done;
  }

  if (npatterns < HTTPHEADER_MIN_ROWS) {
    TEST_LOGF("number of rows, min:%zu actual:%zu\n",
        (size_t)HTTPHEADER_MIN_ROWS,
        npatterns);
    status = TEST_FAIL;
    goto done;
  }

  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOG("failed to compile loaded patterns");
    goto done;
  }

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    size_t inputlen = strlen(tests[i].input);
    ret = reset_match(reset, tests[i].input, inputlen);
    if (ret != RESET_OK) {
      status = TEST_FAIL;
      TEST_LOGF("test:%zu no matches", i);
      continue;
    }

    while ((id = reset_get_next_match(reset)) >= 0) {
      if (tests[i].type == reset_get_type(reset, id) &&
          strcmp(tests[i].name, reset_get_name(reset, id)) == 0) {
        if (tests[i].type == RESET_MATCH_COMPONENT && tests[i].version) {
          const char *version =
              reset_get_substring(reset, id, tests[i].input, inputlen, NULL);
          if (strcmp(tests[i].version, version) == 0) {
            break; /* matched expected type, name, version */
          }
        } else {
          break; /* matched expected type, name */
        }
      }
    }

    if (id < 0) {
      status = TEST_FAIL;
      TEST_LOGF("test:%zu no matches for type:%s name:%s version:%s", i,
          reset_type2str(tests[i].type), tests[i].name,
          tests[i].version ? tests[i].version : "none");
    }
  }

done:
  reset_free(reset);
  return status;
}

TEST_ENTRY(
  {"match", test_match},
  {"noadd", test_noadd},
  {"nomatch", test_nomatch},
  {"substrings", test_substrings},
  {"match_httpheaders", test_match_httpheaders},
)
