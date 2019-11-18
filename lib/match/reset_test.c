/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <string.h>
#include <errno.h>

#include <lib/match/reset.h>
#include <lib/match/reset_csv.h>
#include <lib/util/macros.h>
#include <lib/util/test.h>

#define BANNERS_PATTERN_FILE "./data/pm/banners.pm"
#define HTTPHEADER_PATTERN_FILE "./data/pm/httpheader.pm"
#define HTTPBODY_PATTERN_FILE "./data/pm/httpbody.pm"
#define PFILE_MIN_ROWS 2

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

struct match_test {
  const char *input;
  enum reset_match_type type;
  const char *name;
  const char *version;
};

static int match_patterns(const char *pfile, const struct match_test *tests,
    size_t ntests) {
  size_t npatterns;
  FILE *fp;
  reset_t *reset;
  int status = TEST_OK;
  int ret;
  int id;
  size_t i;

  reset = reset_new();
  if (reset == NULL) {
    TEST_LOG("reset_new failure");
    return TEST_FAIL;
  }

  fp = fopen(pfile, "rb");
  if (!fp) {
    status = TEST_FAIL;
    TEST_LOGF("%s: %s", pfile, strerror(errno));
    goto done;
  }

  ret = reset_csv_load(reset, fp, &npatterns);
  if (ret != RESET_OK) {
    status = TEST_FAIL;
    TEST_LOGF("failed to load patterns from CSV: %s",
        reset_strerror(reset));
    goto done;
  }

  if (npatterns < PFILE_MIN_ROWS) {
    TEST_LOGF("number of rows, min:%zu actual:%zu\n",
        (size_t)PFILE_MIN_ROWS,
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

  for (i = 0; i < ntests; i++) {
    size_t inputlen = strlen(tests[i].input);
    ret = reset_match(reset, tests[i].input, inputlen);
    if (ret != RESET_OK) {
      status = TEST_FAIL;
      TEST_LOGF("test:%zu no matches, expected %s%s%s", i,
          tests[i].name,
          tests[i].version ? " " : "",
          tests[i].version ? tests[i].version : "");
      continue;
    }

    while ((id = reset_get_next_match(reset)) >= 0) {
      if (tests[i].type == reset_get_type(reset, id) &&
          strcmp(tests[i].name, reset_get_name(reset, id)) == 0) {
        if (tests[i].type == RESET_MATCH_COMPONENT && tests[i].version) {
          const char *version =
              reset_get_substring(reset, id, tests[i].input, inputlen, NULL);
          if (version && strcmp(tests[i].version, version) == 0) {
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

static int test_match_banners() {
  static const struct match_test tests[] = {
    {
      .input = "220 (vsFTPd)\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "beasts/vsftpd",
    },
    {
      .input = "220 (vsFTPd 3.0.3)\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "beasts/vsftpd",
      .version = "3.0.3",
    },
    {
      .input = "220 mail.example.com ESMTP Exim 4.89 Fri, 16 Aug 2019 03:31:52 +0000\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "exim/exim",
      .version = "4.89",
    },
    {
      .input = "SSH-2.0-OpenSSH_7.4p1 Debian-10+deb9u6\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "openbsd/openssh",
      .version = "7.4p1",
    },
  };

  return match_patterns(BANNERS_PATTERN_FILE, tests, ARRAY_SIZE(tests));
}
static int test_match_httpheaders() {
  static const struct match_test tests[] = {
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
      .name = "amazon/cloudfront"
    },
    {
      .input = "HTTP/2 301 \r\nserver: CloudFront\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "amazon/cloudfront"
    },
    {
      .input = "HTTP/2 301 \r\nserver: apache",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/http_server"
    },
    {
      .input = "HTTP/2 301 \r\nServer: Apache/2.4.6 (CentOS) OpenSSL/1.0.2k-fips",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/http_server",
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
    {
      .input = "HTTP/2 301 \r\nServer: AkamaiGHost\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "akamai/ghost"
    },
    {
      .input = "HTTP/2 301 \r\nserver: Apache/2.4.6 (Red Hat Enterprise Linux) OpenSSL/1.0.2k-fips PHP/7.1.28",
      .type = RESET_MATCH_COMPONENT,
      .name = "redhat/rhel"
    },
    {
      .input = "HTTP/2 301 \r\nserver: Apache/2.4.6 (Red Hat Enterprise Linux) OpenSSL/1.0.2k-fips PHP/7.1.28",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/http_server",
      .version = "2.4.6",
    },
    {
      .input = "HTTP/2 301 \r\nserver: Apache/2.4.6 (Red Hat Enterprise Linux) OpenSSL/1.0.2k-fips PHP/7.1.28",
      .type = RESET_MATCH_COMPONENT,
      .name = "openssl/openssl",
      .version = "1.0.2k-fips",
    },
    {
      .input = "HTTP/2 301 \r\nserver: Apache/2.4.6 (Red Hat Enterprise Linux) OpenSSL/1.0.2k-fips PHP/7.1.28",
      .type = RESET_MATCH_COMPONENT,
      .name = "php/php",
      .version = "7.1.28",
    },
    {
      .input = "HTTP/2 301 \r\nserver: Apache/2.4.6 (Red Hat Enterprise Linux) OpenSSL/1.0.2k-fips Python/7.2.9",
      .type = RESET_MATCH_COMPONENT,
      .name = "python/python",
      .version = "7.2.9",
    },
    {
      .input = "HTTP/2 301\r\nServer: Apache/2.4.10 (Debian) SVN/1.8.10 mod_fastcgi/mod_fastcgi-SNAP-0910052141 mod_fcgid/2.3.9 mod_python/3.3.1 Python/2.7.9 OpenSSL/1.0.2l",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/http_server",
      .version = "2.4.10",

    },
    {
      .input = "HTTP/2 301\r\nServer: Apache/2.4.10 (Debian) SVN/1.8.10 mod_fastcgi/mod_fastcgi-SNAP-0910052141 mod_fcgid/2.3.9 mod_python/3.3.1 Python/2.7.9 OpenSSL/1.0.2l",
      .type = RESET_MATCH_COMPONENT,
      .name = "openssl/openssl",
      .version = "1.0.2l",

    },
    {
      .input = "HTTP/2 301 \r\nServer: IIS\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "microsoft/iis"
    },
    {
      .input = "HTTP/2 301 \r\nserver: Microsoft-IIS/10.0\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "microsoft/iis",
      .version = "10.0",
    },
    {
      .input = "HTTP/2 301 \r\nserver: Jetty\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "jetty/jetty",
    },
    {
      .input = "HTTP/2 301 \r\nserver: Jetty(9.4.12.v20180830)\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "jetty/jetty",
      .version = "9.4.12.v20180830",
    },
    {
      .input = "HTTP/2 301 \r\nx-powered-by: Next.js\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "nextjs/nextjs",
    },
    {
      .input = "HTTP/2 301 \r\nx-powered-by: Next.js 7.0.2\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "nextjs/nextjs",
      .version = "7.0.2",
    },
    {
      .input = "HTTP/2 301 \r\nServer: openresty/1.13.6.2\r\n",
      .type = RESET_MATCH_COMPONENT,
      .name = "openresty/openresty",
      .version = "1.13.6.2",
    }
  };

  return match_patterns(HTTPHEADER_PATTERN_FILE, tests, ARRAY_SIZE(tests));
}

static int test_match_httpbody() {
  static const struct match_test tests[] = {
    {
      .input = "<meta name=\"generator\" content=\"WordPress 5.2.1\" />",
      .type = RESET_MATCH_COMPONENT,
      .name = "wordpress/wordpress",
      .version = "5.2.1"
    },
    {
      .input = "<address>Apache/2.4.25 (Debian) Server at example.com Port 80</address>",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/http_server",
      .version = "2.4.25"
    },
    {
      .input = "<address>Apache/2.4.25 Server at example.com Port 80</address>",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/http_server",
      .version = "2.4.25"
    },
    { .input = "<address>Apache/2.4.27 (Ubuntu) PHP/5.3.29"
               " Server at example.com Port 80</address>",
      .type = RESET_MATCH_COMPONENT,
      .name = "apache/http_server",
      .version = "2.4.27"
    },
    { .input = "<address>Apache/2.4.27 (Ubuntu) PHP/5.3.29"
               " Server at example.com Port 80</address>",
      .type = RESET_MATCH_COMPONENT,
      .name = "php/php",
      .version = "5.3.29"
    },
    {
      .input = "<meta name=\"generator\" content=\"Drupal 7 (https://www.drupal.org) />",
      .type = RESET_MATCH_COMPONENT,
      .name = "drupal/drupal",
      .version = "7"
    },
    {
      .input = "<meta name=\"Generator\" content=\"Drupal\"/>",
      .type = RESET_MATCH_COMPONENT,
      .name = "drupal/drupal",
    }
  };

  return match_patterns(HTTPBODY_PATTERN_FILE, tests, ARRAY_SIZE(tests));
}

TEST_ENTRY(
  {"match", test_match},
  {"noadd", test_noadd},
  {"nomatch", test_nomatch},
  {"substrings", test_substrings},
  {"match_banners", test_match_banners},
  {"match_httpheaders", test_match_httpheaders},
  {"match_httpbody", test_match_httpbody},
)
