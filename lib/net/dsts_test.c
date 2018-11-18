#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <lib/net/ip.h>
#include <lib/net/dsts.h>

#define MAX_EXPECTED_RANGE 32

static int test_ranges() {
  struct dsts_ctx dsts;
  int res = EXIT_SUCCESS;
  static const struct {
    char *inaddrs;
    char *inports;
    size_t nexpected;
    char *expected[MAX_EXPECTED_RANGE];
  } vals[] = {
    {
      .inaddrs = "",
      .inports = "",
      .nexpected = 0,
    },
    {
      .inaddrs = "",
      .inports = "20",
      .nexpected = 0,
    },
    {
      .inaddrs = "127.0.0.1",
      .inports = "",
      .nexpected = 0,
    },
    {
      .inaddrs = "iamtwelve",
      .inports = "",
      .nexpected = 0,
    },
    {
      .inaddrs = "",
      .inports = "whatisthis",
      .nexpected = 0,
    },
    {
      .inaddrs = "iamtwelve",
      .inports = "whatisthis",
      .nexpected = 0,
    },
    {
      .inaddrs = "127.0.0.1",
      .inports = "80",
      .nexpected = 1,
      .expected = {
        "127.0.0.1", "80",
      },
    },
    {
      .inaddrs = "127.0.0.1-127.0.0.2",
      .inports = "80,443",
      .nexpected = 4,
      .expected = {
        "127.0.0.1", "80",
        "127.0.0.1", "443",
        "127.0.0.2", "80",
        "127.0.0.2", "443",
      },
    },
    {NULL, NULL},
  };
  size_t i;
  size_t j;
  ip_addr_t addr;
  socklen_t addrlen = 0;
  int ret;
  char hostbuf[128];
  char portbuf[24];

  for (i=0; vals[i].inaddrs != NULL; i++) {
    j = 0;
    dsts_init(&dsts);
    dsts_add(&dsts, vals[i].inaddrs, vals[i].inports, NULL);
    while (dsts_next(&dsts, &addr.u.sa, &addrlen, NULL)) {
      if (j+1 > vals[i].nexpected) {
        fprintf(stderr, "  %zu,%zu: nexpected:%zu nactual:%zu\n", i, j,
            vals[i].nexpected, j);
        res = EXIT_FAILURE;
        break;
      }

      ret = getnameinfo(&addr.u.sa, addrlen, hostbuf, sizeof(hostbuf),
          portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
      if (ret != 0) {
        fprintf(stderr, "  %zu,%zu: getnameinfo: %s\n", i, j,
            gai_strerror(ret));
        res = EXIT_FAILURE;
        break;
      }

      if (strcmp(vals[i].expected[j*2], hostbuf) != 0) {
        fprintf(stderr, "  %zu,%zu: expected host: %s, was: %s\n", i, j,
            vals[i].expected[j*2], hostbuf);
        res = EXIT_FAILURE;
      } else if (strcmp(vals[i].expected[j*2 + 1], portbuf) != 0) {
        fprintf(stderr, "  %zu,%zu: expected port: %s, was: %s\n", i, j,
            vals[i].expected[j*2+1], portbuf);
        res = EXIT_FAILURE;
      }
      j++;
    }

    if (j != vals[i].nexpected) {
      fprintf(stderr, "  %zu: post nexpected:%zu nactual:%zu\n", i,
          vals[i].nexpected, j);
      res = EXIT_FAILURE;
    }

    dsts_cleanup(&dsts);
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
    {"ranges", test_ranges},
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

