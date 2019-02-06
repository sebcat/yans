#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <lib/util/objalloc.h>
#include <lib/util/objtbl.h>

#define LOG_FAILURE() \
    fprintf(stderr, "%s:%d test failure\n", __FILE__, __LINE__)

#define LOG_FAILUREF(fmt, ...) \
    fprintf(stderr, "%s:%d test failure: " fmt "\n", __FILE__, __LINE__, \
        __VA_ARGS__)

/* A little bit dirty to have a goto inside a macro like this, but it will
 * do for now... */
#define ASSERT_SIZE(tbl_, size_)            \
    do {                                    \
      if (objtbl_size((tbl_)) != (size_)) { \
        LOG_FAILURE();                      \
        goto end_objtbl_cleanup;            \
      }                                     \
    } while(0)

/* testobj --
 *   Data type for testing objtbl with composite data types */
struct testobj {
  char key[32];
  int value;
};

struct opts {
  struct objtbl_opts tblopts;
  char **tests;
  int ntests;
  int dump_tables;
};

struct testobj test_values[] = {
  {.key = "a", .value = 'a'},
  {.key = "b", .value = 'b'},
  {.key = "c", .value = 'c'},
  {.key = "d", .value = 'd'},
  {.key = "", .value  = 0x29a},
  {.key = "aardvark", .value = 0x822f04f8},
  {.key = "antilope", .value = 0xe8155374},
  {.key = "zebra", .value = 0xa2706988},
  {.key = "protozoan", .value = 0x39faa2da},
  {.key = "axolotl", .value = 0xe3436e76},
  {.key = "lynx", .value = 0xec91a0ea},
  {.key = "wolverine", .value = 0xffd9771e},
  {.key = "whale", .value = 0x0f92f69e},
  {.key = "micelle", .value = 0x85f17eae},
  {.key = "aa", .value = 0xe3df910e},
  {.key = "spacebar", .value = 0x0f3ae434},
  {.key = "ectoplasm", .value = 0xc85aa376},
  {.key = "hera", .value = 0x9bebe642},
  {.key = "hades", .value = 0x894732c4},
  {.key = "ares", .value = 0x9617f958},
  {.key = "athena", .value = 0x02b96812},
  {.key = "themis", .value = 0x02b96812},
  {.key = "aegis", .value = 0x06921ac8},
  {.key = "aspis", .value = 0x06921ac0},
  {.key = "bakunin", .value = 0xa89de7d8},
  {.key = "proudhon", .value = 0x09637394},
  {.key = "hayek", .value = 0x09637390},
  {.key = "dirty", .value = 0x93fb596e},
  {.key = "bomb", .value = 0x10431610},
  {.key = "dictionary", .value = 0xdc1466e2},
  {.key = "sweep", .value = 0x2bc28d12},
  {.key = "ionosphere", .value = 0x53c8c932},
  {.key = "military intelligence", .value = 0xffdf4214},
  {.key = "oxymoron", .value = 0x6aa58d66},
  {.key = "PGP", .value = 0x2b3fa11c},
  {.key = "PEM", .value = 0xc4112e08},
  {.key = "RSA", .value = 0x21b40834},
  {.key = "opsec", .value = 0x804d2e4e},
  {.key = "when", .value = 0x46403300},
  {.key = "the", .value = 0x09c95ba8},
  {.key = "elks", .value = 0x22bd97d6},
  {.key = "clap", .value = 0x803ad5da},
  {.key = "their", .value = 0x19043fdc},
  {.key = "hands", .value = 0x0601064e},
  {.key = "blood", .value = 0x6ccc76c4},
  {.key = "seeps", .value = 0x18ddb6fc},
  {.key = "from", .value = 0x1593fcec},
  {.key = "the mountains", .value = 0xf2bdf572},
  {.key = "\r\n\t ", .value = 0xfcbd480a},
  {.key = " ", .value = 0xddeea064},
  {.key = "  ", .value = 0x8ea9dbb4},
  {.key = "   ", .value = 0x86047ace},
  {.key = "\t", .value = 0x740699bc},
  {.key = "\t\t", .value = 0x916343c6},
  {.key = "\t\t\t", .value = 0x8c969596},
  {.key = "ttt", .value = 0x825b3e72},
  {.key = "pogrom", .value = 0x3a012ba0},
  {.key = "starve", .value = 0x95fa092a},
  {.key = "starvation", .value = 0x165697e2},
  {.key = "VCR", .value = 0x4a4441b0},
  {.key = "inb4", .value = 0x720e700c},
  {.key = "accidentally", .value = 0x421924f4},
  {.key = "static", .value = 0xce72ba8e},
  {.key = "line", .value = 0xc94cabfe},
  {.key = "lanyard", .value = 0x9e0d0ae2},
  {.key = "bridle", .value = 0x71edb220},
  {.key = "riser", .value = 0xacbeb25a},
  {.key = "drogue", .value = 0x35fee028},
  {.key = "toggle", .value = 0x4999da4e},
  {.key = "stairstep", .value = 0x7c3f1328},
  {.key = "diamond", .value = 0xd83c1636},
  {.key = "chevron", .value = 0x6c577224},
  {.key = "pieces", .value = 0xfc79e902},
  {.key = "wedge", .value = 0xf1405612},
  {.key = "accordion", .value = 0x72587872},
  {.key = "doughnut", .value = 0x69465fc6},
  {.key = "television", .value = 0x0a1e0d0e},
  {.key = "set", .value = 0x87ec7560},
  {.key = "remote", .value = 0x5c045538},
  {.key = "control", .value = 0x06db5518},
  {.key = "coffee", .value = 0x65db15c4},
  {.key = "tea", .value = 0xeaae30ec},
  {.key = "cookies", .value = 0x8cdfcfe6},
  {.key = "and", .value = 0xd9e1af80},
  {.key = "cream", .value = 0x7c1174b2},
  {.key = "vinterland", .value = 0x1976657c},
  {.key = "hinterland", .value = 0x5c3cedf2},
  {.key = "balcony", .value = 0xb612a052},
  {.key = "plants", .value = 0xaf706ea6},
  {.key = "so many plants", .value = 0x729385d4},
  {.key = "oh dear god", .value = 0x0056e202},
  {.key = "why am I", .value = 0x217edeb2},
  {.key = "not automating", .value = 0xb4acaac8},
  {.key = "this process", .value = 0x9e63ece4},
  {.key = "using a", .value = 0x936c90d2},
  {.key = "dictionary?", .value = 0x616b5d22},
  {.key = "oh well,", .value = 0x67b098e4},
  {.key = "ALL GLORY TO THE HYPNOTOAD", .value = 0x0e54a4b6},
};

/* 32-bit FNV1a constants */
#define FNV1A_OFFSET 0x811c9dc5
#define FNV1A_PRIME   0x1000193

static objtbl_hash_t hashfunc(void *obj, objtbl_hash_t seed) {
  struct testobj *to = obj;
  objtbl_hash_t hash = seed;
  int i;
  size_t len;

  /* test hash bounds */
  if (to->key[0] == 'a' && to->key[1] == '\0') {
    return (objtbl_hash_t)0;
  } else if (to->key[0] == 'b' && to->key[1] == '\0') {
    return (objtbl_hash_t)-1;
  } else if (to->key[0] == 'c' && to->key[1] == '\0') {
    return (objtbl_hash_t)INT_MAX;
  } else if (to->key[0] == 'd' && to->key[1] == '\0') {
    return (objtbl_hash_t)INT_MAX+1;
  }

  /* FNV1a hash the key! */
  len = strlen(to->key);
  for (i = 0; i < len; i++) {
    hash = (hash ^ to->key[i]) * FNV1A_PRIME;
  }

  return hash;
}

static int cmpfunc(void *left, void *right) {
  struct testobj *a = left;
  struct testobj *b = right;

  return strcmp(a->key, b->key);
}

/* test initialization and cleanup, with varying # of slots and seed  */
static int test_init_cleanup(struct opts *opts) {
  uint32_t i;
  struct objtbl_ctx objtbl;
  struct objtbl_opts tblopts = {0};
  int ret;

  for (i = 0; i < 10; i++) {
    tblopts.hashseed = i * 3;
    ret = objtbl_init(&objtbl, &tblopts, i);
    objtbl_cleanup(&objtbl);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      return EXIT_FAILURE;
    }
  }

  for (i = 0; i < 10; i++) {
    tblopts.hashseed = i;
    ret = objtbl_init(&objtbl, &tblopts, i*3);
    objtbl_cleanup(&objtbl);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

/* test simple insertion and lookup using contains */
static int test_insert_contains(struct opts *opts) {
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;
  struct testobj noexist = {.key = "wololo", .value = 2};
  struct testobj exist   = {.key = "iexist", .value = 0x29a};

  ret = objtbl_init(&objtbl, &opts->tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&objtbl, 0);
  if (objtbl_contains(&objtbl, &noexist)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* insert a value in objtbl */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, &exist);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&objtbl, 1);
  if (objtbl_contains(&objtbl, &noexist)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  ASSERT_SIZE(&objtbl, 1);
  result = EXIT_SUCCESS;
end_objtbl_cleanup:
  objtbl_cleanup(&objtbl);
end:
  return result;
}

/* test insertion with duplicate keys */
static int test_insert_duplicate(struct opts *opts) {
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;
  struct testobj valA   = {.key = "wololo", .value = 2};
  struct testobj valB   = {.key = "wololo", .value = 0x29a};
  struct testobj keyobj = {.key = "wololo"};
  void *resptr = 0;
  struct testobj *res;

  ret = objtbl_init(&objtbl, &opts->tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* insert a value in objtbl */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, (void *)&valA);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* check existance of inserted element */
  ASSERT_SIZE(&objtbl, 1);
  if (!objtbl_contains(&objtbl, (void *)&valA)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the inserted element */
  ASSERT_SIZE(&objtbl, 1);
  ret = objtbl_get(&objtbl, (void *)&keyobj, &resptr);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* check value of retrieved element */
  res = (struct testobj *)resptr;
  if (res->value != valA.value) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* insert a value in objtbl with a duplicate key */
  ASSERT_SIZE(&objtbl, 1);
  ret = objtbl_insert(&objtbl, (void *)&valB);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the inserted element, now expected to have replaced the
   * previously inserted element  */
  resptr = 0;
  ASSERT_SIZE(&objtbl, 1);
  ret = objtbl_get(&objtbl, (void *)&keyobj, &resptr);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* check value of retrieved element */
  res = (struct testobj *)resptr;
  if (res->value != valB.value) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  ASSERT_SIZE(&objtbl, 1);
  result = EXIT_SUCCESS;
end_objtbl_cleanup:
  objtbl_cleanup(&objtbl);
end:
  return result;
}


/* test simple insertion and lookup using contains, with a NULL value */
static int test_insert_null(struct opts *opts) {
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;

  ret = objtbl_init(&objtbl, &opts->tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* Try to insert NULL */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, NULL);
  if (ret != OBJTBL_EINVAL) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* Make sure the table is still empty */
  ASSERT_SIZE(&objtbl, 0);

  result = EXIT_SUCCESS;
end_objtbl_cleanup:
  objtbl_cleanup(&objtbl);
end:
  return result;
}


/* test insertion and lookup, with table rehash */
static int test_insert_get_rehash(struct opts *opts) {
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;
  uint32_t i;
  uint32_t ntest_values = sizeof(test_values) / sizeof(*test_values);
  void *val = 0;
  struct testobj *typval;

  /* initialize the table, with an "expected" element count of four */
  ret = objtbl_init(&objtbl, &opts->tblopts, 4);
  if (ret != OBJTBL_OK) {
    goto end;
  }

  /* insert a bunch of test values */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_insert(&objtbl, &test_values[i]);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* retrieve and validate table content */
  ASSERT_SIZE(&objtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, &test_values[i], &val);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }

    typval = val;
    if (typval->value != test_values[i].value) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  result = EXIT_SUCCESS;
end_objtbl_cleanup:
  objtbl_cleanup(&objtbl);
end:
  return result;
}

static int test_get_insert_get_remove_get(struct opts *opts) {
  struct objtbl_ctx objtbl;
  uint32_t i;
  uint32_t ntest_values = sizeof(test_values) / sizeof(*test_values);
  int result = EXIT_FAILURE;
  void *val;
  int ret;
  struct testobj *typval;

  ret = objtbl_init(&objtbl, &opts->tblopts, 8);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* retrieve non-existent content */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, &test_values[i], &val);
    if (ret != OBJTBL_ENOTFOUND) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* insert table content */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_insert(&objtbl, &test_values[i]);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* retrieve existing table content and validate result */
  ASSERT_SIZE(&objtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, &test_values[i], &val);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }

    typval = val;
    if (typval->value != test_values[i].value) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* remove existing table content */
  ASSERT_SIZE(&objtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_remove(&objtbl, &test_values[i]);
    if (ret != OBJTBL_OK) {
      objtbl_dump(&objtbl, stderr);
      LOG_FAILUREF("index:%u", i);
      goto end_objtbl_cleanup;
    }
  }

  /* retrieve non-existent content */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, &test_values[i], &val);
    if (ret != OBJTBL_ENOTFOUND) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  result = EXIT_SUCCESS;
end_objtbl_cleanup:
  objtbl_cleanup(&objtbl);
end:
  return result;
}

static int test_stats(struct opts *opts) {
  struct objtbl_stats stats;
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;
  size_t i;
  size_t nelems;
  struct objalloc_ctx objmem;
  struct objalloc_chunk *chk;
  struct testobj *val;

  srand((unsigned int)opts->tblopts.hashseed);
  objalloc_init(&objmem, 4096);

  for (nelems = 100;  nelems <= 100000; nelems *= 10) {
    ret = objtbl_init(&objtbl, &opts->tblopts, 8);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end;
    }

    for (i = 0; i < nelems; i++) {
      chk = objalloc_alloc(&objmem, sizeof(struct testobj));
      val = (struct testobj *)chk->data;
      val->value = rand();
      snprintf(val->key, sizeof(val->key), "%x", val->value);
      ret = objtbl_insert(&objtbl, val);
      if (ret != OBJTBL_OK) {
        LOG_FAILURE();
        goto end_objtbl_cleanup;
      }
    }

    ret = objtbl_calc_stats(&objtbl, &stats);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }

    printf("    rnd-%zu: nbytes:%zu load:%.2f avg:%.2f mean:%u max:%u\n"
        , nelems, stats.nbytes, (double)stats.size / (double)stats.cap,
        stats.average_probe_distance, stats.mean_probe_distance,
        stats.max_probe_distance);
    if (opts->dump_tables) {
      objtbl_dump(&objtbl, stderr);
    }

    objtbl_cleanup(&objtbl);

    ret = objtbl_init(&objtbl, &opts->tblopts, 8);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end;
    }

    for (i = 0; i < nelems; i++) {
      chk = objalloc_alloc(&objmem, sizeof(struct testobj));
      val = (struct testobj *)chk->data;
      val->value = rand();
      snprintf(val->key, sizeof(val->key), "%x", val->value);
      ret = objtbl_insert(&objtbl, val);
      if (ret != OBJTBL_OK) {
        LOG_FAILURE();
        goto end_objtbl_cleanup;
      }
    }

    ret = objtbl_calc_stats(&objtbl, &stats);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }

    printf("    seq-%zu: nbytes:%zu load:%.2f avg:%.2f mean:%u max:%u\n"
        , nelems, stats.nbytes, (double)stats.size / (double)stats.cap,
        stats.average_probe_distance, stats.mean_probe_distance,
        stats.max_probe_distance);
    if (opts->dump_tables) {
      objtbl_dump(&objtbl, stderr);
    }

    objtbl_cleanup(&objtbl);
  }

  result = EXIT_SUCCESS;
end:
  objalloc_cleanup(&objmem);
  return result;
end_objtbl_cleanup:
  objtbl_cleanup(&objtbl);
  objalloc_cleanup(&objmem);
  return result;
}

static void opts_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  static const char *optstr = "hds:";
  struct timespec tp = {0};
  struct option lopts[] = {
    {"seed", required_argument, NULL, 's'},
    {"dump", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  clock_gettime(CLOCK_MONOTONIC, &tp);
  opts->tblopts.hashseed = tp.tv_nsec;
  opts->dump_tables = 0;
  opts->tblopts.hashfunc = hashfunc;
  opts->tblopts.cmpfunc = cmpfunc;

  while ((ch = getopt_long(argc, argv, optstr, lopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      opts->tblopts.hashseed = (uint32_t)strtoul(optarg, NULL, 10);
      break;
    case 'd':
      opts->dump_tables = 1;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  opts->ntests = argc - optind;
  opts->tests = argv + optind;

  return;

usage:
  fprintf(stderr,
      "%s\n"
      "options:\n"
      "  -s|--seed <n>         seed to use for tables\n"
      "  -d|--dump             dump stats tables\n"
      , argv[0]);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  size_t i;
  int ret;
  int result = EXIT_SUCCESS;
  int done = 0;
  struct opts opts;
  struct {
    const char *name;
    int (*func)(struct opts *);
  } tests[] = {
    {"init_cleanup", test_init_cleanup},
    {"insert", test_insert_contains},
    {"insert_duplicate", test_insert_duplicate},
    {"insert_null", test_insert_null},
    {"insert_get_rehash", test_insert_get_rehash},
    {"get_insert_get_remove_get", test_get_insert_get_remove_get},
    {"stats", test_stats},
  };

  /* load and print options */
  opts_or_die(&opts, argc, argv);
  printf("    seed:%u\n", opts.tblopts.hashseed);

  for (i = 0; !done && i < sizeof(tests) / sizeof(*tests); i++) {
    if (opts.ntests > 0) {
      /* we have explicitly set test cases to run */
      if (strcmp(*opts.tests, tests[i].name) != 0) {
        continue;
      }

      /* the name matched - run this test and check if it's the last test
       * to run */
      opts.ntests--;
      opts.tests++;
      if (opts.ntests == 0) {
        done = 1;
      }
    }

    ret = tests[i].func(&opts);
    if (ret == EXIT_SUCCESS) {
      printf("OK  %s\n", tests[i].name);
    } else {
      printf("ERR %s\n", tests[i].name);
      result = EXIT_FAILURE;
    }
  }

  return result;
}
