#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>

#include <lib/match/reset.h>
#include <lib/util/macros.h>

/* Increase MAX_* as needed */
#define MAX_PATTERN_TESTS  8

/* Categories are based on the data we match against, not what type of
 * match we are looking for. We should ideally only need one DFA for each
 * type of data we match against.
 *
 * There should be no need to look for Postfix SMTP servers in HTTP
 * response bodies (that would give false positives due to context), but
 * we don't really care if the response body tells us that a specific
 * component is installed on the tested service or if there's a stack
 * trace, or if there's a directory listing at the matching phase. */
enum category_id {
  CATEGORY_UNKNOWN      = 0,
  CATEGORY_HTTPHEADER   = 1,
  /* TODO: NONHTTP_BANNER for banner matching */
  CATEGORY_MAX
};

enum match_type {
  MATCH_UNKNOWN       = 0,
  MATCH_COMPONENT     = 1,
  MATCH_MAX,
};

enum component_id {
  COMPONENT_UNKNOWN,
  COMPONENT_NGINX,
  COMPONENT_DRUPAL,
  COMPONENT_MAGENTO,
  COMPONENT_ASP_NET,
  COMPONENT_EASY_ENGINE,
  COMPONENT_LITESPEED,
  COMPONENT_PHP,
  COMPONENT_AWSELB,
  COMPONENT_S3,
  COMPONENT_CLOUDFLARE,
  COMPONENT_CLOUDFRONT,
  COMPONENT_APACHE,
  COMPONENT_CENTOS,
  COMPONENT_DEBIAN,
  COMPONENT_UBUNTU,
  COMPONENT_OPENSSL,
  COMPONENT_MAX
};

struct pgen_test {
  const char *data;
  unsigned long len; /* if > 0: data is counted, otherwise \0-terminated */
  const char *expected_version; /* if !NULL: expected version string */
};

struct pgen_pattern {
  int id;
  enum match_type type;
  enum component_id component;
  const char *pattern;
  struct pgen_test positives[MAX_PATTERN_TESTS];
  struct pgen_test negatives[MAX_PATTERN_TESTS];
};

struct pgen_categories {
  enum category_id id;
  struct pgen_pattern *patterns;
  size_t npatterns;
};

struct opts {
  int check;
};

/* TODO: long term, the data we have here could be loaded from a file
 *       created by e.g., a web UI for easier updating of patterns */
static struct pgen_pattern httpheader_[] = {
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_NGINX,
    .pattern = "\\r?\\n[Ss]erver: ?nginx/?([0-9.]+)?",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nserver: nginx\r\n",
      },
      {
        .data = "HTTP/2 301 \r\nServer: nginx/1.14.2\r\n",
        .expected_version = "1.14.2",
      }
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_DRUPAL,
    .pattern = "\r?\n[Xx]-[Gg]enerator: ?[Dd]rupal ?([0-9.]+)?",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nx-generator: drupal\r\n",
      },
      {
        .data = "HTTP/2 301 \r\nX-Generator: Drupal 8 (https://www.drupal.org)\r\n",
        .expected_version = "8",
      }
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_DRUPAL,
    .pattern = "\r?\n[Xx]-[Dd]rupal",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nx-drupal-cache: HIT\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_MAGENTO,
    .pattern = "\r?\n[Xx]-[Mm]agento",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nx-magento-cloud-cache: MISS\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_ASP_NET,
    .pattern = "\r?\n[Xx]-[Pp]owered-[Bb]y: ?ASP.NET",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nx-powered-by: ASP.NET\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_EASY_ENGINE,
    .pattern = "\r?\n[Xx]-[Pp]owered-[Bb]y: ?[Ee]asy[Ee]ngine ?([0-9.]+)?",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nx-powered-by: EasyEngine\r\n",
      },
      {
        .data = "HTTP/2 301 \r\nX-Powered-By: EasyEngine 3.8.1\r\n",
        .expected_version = "3.8.1",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_LITESPEED,
    .pattern = "\r?\n[Ss]erver: ?[Ll]ite[Ss]peed",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nserver: litespeed\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_LITESPEED,
    .pattern = "\r?\n[Xx]-[Ll]ite[Ss]peed",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nx-litespeed-cache: hit\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_PHP,
    .pattern = "\r?\n[Xx]-[Pp]owered-[Bb]y: ?PHP/?([0-9.]+)?",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nx-powered-by: PHP\r\n",
      },
      {
        .data = "HTTP/2 301 \r\nX-Powered-By: PHP/7.2.5\r\n",
        .expected_version = "7.2.5",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_AWSELB,
    .pattern = "\r?\n[Ss]erver: ?awselb/?([0-9.]+)?",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: awselb\r\n",
      },
      {
        .data = "HTTP/2 301 \r\nserver: awselb/2.0\r\n",
        .expected_version = "2.0",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_S3,
    .pattern = "\r?\n[Ss]erver: ?[Aa]mazon[sS]3",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: AmazonS3\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_CLOUDFLARE,
    .pattern = "\r?\n[Ss]erver: ?[Cc]loud[Ff]lare",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: cloudflare\r\n",
      },
      {
        .data = "HTTP/2 301 \r\nserver: cloudflare\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_CLOUDFRONT,
    .pattern = "\r?\n[Ss]erver: ?[Cc]loud[Ff]ront",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: cloudfront\r\n",
      },
      {
        .data = "HTTP/2 301 \r\nserver: CloudFront\r\n",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_APACHE,
    .pattern = "\r?\n[Ss]erver: ?[Aa]pache/?([0-9.]+)?",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nserver: apache",
      },
      {
        .data = "HTTP/2 301 \r\nServer: Apache/2.4.6 (CentOS) OpenSSL/1.0.2k-fips",
        .expected_version = "2.4.6",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_CENTOS,
    .pattern = "\r?\n[Ss]erver:[^\r\n]+\\([Cc]ent[Oo][Ss]\\)",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: Apache/2.4.6 (CentOS) OpenSSL/1.0.2k-fips",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_DEBIAN,
    .pattern = "\r?\n[Ss]erver:[^\r\n]+\\([Dd]ebian\\)",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: Apache/2.4.6 (Debian) OpenSSL/1.0.2k-fips",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_UBUNTU,
    .pattern = "\r?\n[Ss]erver:[^\r\n]+\\([Uu]buntu\\)",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: Apache/2.4.6 (Ubuntu) OpenSSL/1.0.2k-fips",
      },
    }
  },
  {
    .type      = MATCH_COMPONENT,
    .component = COMPONENT_OPENSSL,
    .pattern = "\r?\n[Ss]erver:[^\r\n]+[Oo]pen[Ss][Ss][Ll]/?([0-9][A-Za-z0-9.-]+)?",
    .positives = {
      {
        .data = "HTTP/2 301 \r\nServer: Apache/2.4.6 (Ubuntu) OpenSSL/1.0.2k-fips",
        .expected_version = "1.0.2k-fips",
      },
      {
        .data = "HTTP/2 301 \r\nServer: /2.4.6 (Ubuntu) OpenSSL",
      },
    }
  }
};

static struct pgen_categories categories_[] = {
  {CATEGORY_HTTPHEADER, httpheader_, ARRAY_SIZE(httpheader_)},
  /* TODO: components for smtp, imap, ... */
};

const char *category_name(enum category_id id) {
  static const char *names[CATEGORY_MAX] = {
    [CATEGORY_UNKNOWN]    = "UNKNOWN",
    [CATEGORY_HTTPHEADER] = "HTTPHEADER",
  };

  if (id < 0 || id >= CATEGORY_MAX) {
    id = CATEGORY_UNKNOWN;
  }

  return names[id] ? names[id] : names[CATEGORY_UNKNOWN];
}

/* names should be vendor/product, with no spaces and only small letters.
 * there should be only one / in the string for ease of parsing later on */
const char *component_name(enum component_id id) {
  static const char *names[COMPONENT_MAX] = {
    [COMPONENT_UNKNOWN]      = "unknown/unknown",
    [COMPONENT_NGINX]        = "nginx/nginx",
    [COMPONENT_DRUPAL]       = "drupal/drupal",
    [COMPONENT_MAGENTO]      = "magento/magento",
    [COMPONENT_ASP_NET]      = "microsoft/asp.net",
    [COMPONENT_EASY_ENGINE]  = "easyengine/easyengine",
    [COMPONENT_LITESPEED]    = "litespeed/litespeed",
    [COMPONENT_PHP]          = "php/php",
    [COMPONENT_AWSELB]       = "amazon/awselb",
    [COMPONENT_S3]           = "amazon/s3",
    [COMPONENT_CLOUDFLARE]   = "cloudflare/cloudflare",
    [COMPONENT_CLOUDFRONT]   = "cloudflare/cloudfront",
    [COMPONENT_APACHE]       = "apache/apache",
    [COMPONENT_CENTOS]       = "redhat/centos",
    [COMPONENT_DEBIAN]       = "debian/debian",
    [COMPONENT_UBUNTU]       = "canonical/ubuntu",
    [COMPONENT_OPENSSL]      = "openssl/openssl",
  };

  if (id < 0 || id >= COMPONENT_MAX) {
    id = COMPONENT_UNKNOWN;
  }

  return names[id] ? names[id] : names[COMPONENT_UNKNOWN];
}

static void opts_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  const char *argv0 = argv[0];
  const char *optstr = "ch";
  struct option options[] = {
    {"check", no_argument, NULL, 'c'},
    {"help",  no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc, argv, optstr, options, NULL)) != -1) {
    switch (ch) {
    case 'c':
      opts->check = 1;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  return;
usage:
  fprintf(stderr, "usage: %s [--check]\n", argv0);
  exit(EXIT_FAILURE);
}

static int check(void) {
  size_t cat;
  size_t pat;
  size_t tst;
  size_t len;
  int ret;
  int id;
  int found;
  struct pgen_pattern *pattern;
  struct reset_t *reset;
  int status = 0;
  struct rusage ru = {{0}};
  int npatterns = 0;
  const char *ver;

  reset = reset_new();

  /* Add all the patterns to the set */
  for (cat = 0; cat < ARRAY_SIZE(categories_); cat++) {
    for (pat = 0; pat < categories_[cat].npatterns; pat++) {
      pattern = &categories_[cat].patterns[pat];
      ret = reset_add(reset, pattern->pattern);
      if (ret == RESET_ERR) {
        fprintf(stderr, "  cat:%s component:%s pattern:%zu failure:%s\n",
            category_name(categories_[cat].id),
            component_name(pattern->component),
            pat, reset_strerror(reset));
        status = -1;
      }

      npatterns++;
      pattern->id = ret;
    }
  }

  ret = reset_compile(reset);
  if (ret != RESET_OK) {
    fprintf(stderr, "  reset_compile failure: %s\n", reset_strerror(reset));
    status = -1;
  }

  /* Test positive test cases */
  for (cat = 0; cat < ARRAY_SIZE(categories_); cat++) {
    for (pat = 0; pat < categories_[cat].npatterns; pat++) {
      pattern = &categories_[cat].patterns[pat];
      for (tst = 0;
           tst < MAX_PATTERN_TESTS && pattern->positives[tst].data;
           tst++) {
        len = pattern->positives[tst].len ?
            pattern->positives[tst].len : 
            strlen(pattern->positives[tst].data);
        ret = reset_match(reset, pattern->positives[tst].data, len);
        if (ret != RESET_OK) {
          fprintf(stderr,
              "  cat:%s component:%s positive:%zu failure:%s\n",
              category_name(categories_[cat].id),
              component_name(pattern->component),
              tst, reset_strerror(reset));
          status = -1;
          continue;
        }

        found = 0;
        while ((id = reset_get_next_match(reset)) >= 0) {
          if (id == pattern->id) {
            found = 1;
            break;
          }
        }

        if (!found) {
          fprintf(stderr,
              "  cat:%s component:%s positive:%zu mismatch\n",
              category_name(categories_[cat].id),
              component_name(pattern->component), tst);
          status = -1;
          continue;
        }

        if (pattern->positives[tst].expected_version) {
          ver = reset_get_substring(reset, id, 
              pattern->positives[tst].data, len, NULL);
          if (ver) {
            if (strcmp(pattern->positives[tst].expected_version, ver)) {
            fprintf(stderr,
                "  cat:%s component:%s positive:%zu "
                "version expected:%s actual:%s\n",
                category_name(categories_[cat].id),
                component_name(pattern->component), tst,
                pattern->positives[tst].expected_version, ver);
            }
          } else {
            fprintf(stderr,
                "  cat:%s component:%s positive:%zu no version\n",
                category_name(categories_[cat].id),
                component_name(pattern->component), tst);
          }
        }
      }
    }
  }

  /* Test negative test cases */
  for (cat = 0; cat < ARRAY_SIZE(categories_); cat++) {
    for (pat = 0; pat < categories_[cat].npatterns; pat++) {
      pattern = &categories_[cat].patterns[pat];
      for (tst = 0;
           tst < MAX_PATTERN_TESTS && pattern->negatives[tst].data;
           tst++) {
        len = pattern->negatives[tst].len ?
            pattern->negatives[tst].len : 
            strlen(pattern->negatives[tst].data);
        ret = reset_match(reset, pattern->negatives[tst].data, len);
        if (ret != RESET_OK) {
          continue;
        }

        found = 0;
        while ((id = reset_get_next_match(reset)) >= 0) {
          if (id == pattern->id) {
            found = 1;
            break;
          }
        }

        if (found) {
          fprintf(stderr,
              "  cat:%s component:%s negative:%zu unexpected match\n",
              category_name(categories_[cat].id),
              component_name(pattern->component),
              tst);
          status = -1;
        }
      }
    }
  }

  ret = getrusage(RUSAGE_SELF, &ru);
  if (ret != 0) {
    perror("  rusage");
    status = -1;
  }

  printf("  npatterns:%d user:%lu.%.6lus system:%lu.%.6lus maxrss:%ld kB\n",
    npatterns,
    ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
    ru.ru_stime.tv_sec, ru.ru_stime.tv_usec,
    ru.ru_maxrss);

  reset_free(reset);
  return status;
}

int main(int argc, char *argv[]) {
  int status = EXIT_FAILURE;
  struct opts opts = {0};
  int ret;

  opts_or_die(&opts, argc, argv);
  if (opts.check) {
    ret = check();
    if (ret < 0) {
      fprintf(stderr, "ERR check\n");
      goto done;
    } else {
      fprintf(stderr, "OK  check\n");
    }
  }

  status = EXIT_SUCCESS;
done:
  return status;
}

/*

builds:

struct matcher_component {
  const char *vendor;
  const char *product;
};

//matcher -> httpbodymatcher, httpheadermatcher, bannermatcher ...
typedef struct matcher_ctx matcher_ctx;

int matcher_init(struct matcher_ctx *ctx);
int matcher_load(struct matcher_ctx *ctx, 
int matcher_cleanup(struct matcher_ctx *ctx);
int matcher_match_next(struct matcher_ctx *ctx, struct component_data *out);
int matcher_reset(struct matcher_ctx *ctx);




struct matcher_pattern {
  unsigned long component;
  const char *re;
};

static struct matcher_component components_[] = {
  ...
};

static struct matcher_pattern patterns_[] = {
  {"foo", 0},
  {"bar", 0},
  {"baz", 1},
  ...
};


*/
