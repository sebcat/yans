#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>

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
  char *key;
  int value;
};

struct opts {
  struct objtbl_opts tblopts;
  char **tests;
  int ntests;
  int dump_tables;
};

static uint32_t test_values[] = {
  0x00000000, 0xfffffffe, 0x7fffffff, 0x80000000, 0xb70e7ace, 0x550d8d32,
  0x822f04f8, 0xe8155374, 0xa2706988, 0x39faa2da, 0x0f3ae434, 0xe3436e76,
  0xec91a0ea, 0xffd9771e, 0x0f92f69e, 0x85f17eae, 0xe3df910e, 0xc85aa376,
  0x9bebe642, 0x894732c4, 0x9617f958, 0x02b96812, 0x06921ac8, 0x0e54a4b6,
  0xa89de7d8, 0x09637394, 0x93fb596e, 0x10431610, 0xdc1466e2, 0x2bc28d12,
  0x53c8c932, 0xffdf4214, 0x6aa58d66, 0x2b3fa11c, 0xc4112e08, 0x21b40834,
  0x804d2e4e, 0x46403300, 0x09c95ba8, 0x22bd97d6, 0x803ad5da, 0x19043fdc,
  0x0601064e, 0x6ccc76c4, 0x18ddb6fc, 0x1593fcec, 0xf2bdf572, 0xfcbd480a,
  0xddeea064, 0x8ea9dbb4, 0x86047ace, 0x740699bc, 0x916343c6, 0x8c969596,
  0x825b3e72, 0x3a012ba0, 0x95fa092a, 0x165697e2, 0x4a4441b0, 0x720e700c,
  0x421924f4, 0x9e0d0ae2, 0x71edb220, 0xacbeb25a, 0xc94cabfe, 0x35fee028,
  0xce72ba8e, 0x4999da4e, 0x7c3f1328, 0xd83c1636, 0x6c577224, 0xfc79e902,
  0xf1405612, 0x72587872, 0x69465fc6, 0x0a1e0d0e, 0x87ec7560, 0x5c045538,
  0x06db5518, 0x65db15c4, 0xeaae30ec, 0x8cdfcfe6, 0xd9e1af80, 0x7c1174b2,
  0x1976657c, 0x5c3cedf2, 0xb612a052, 0xaf706ea6, 0x729385d4, 0x0056e202,
  0x217edeb2, 0xb4acaac8, 0x9e63ece4, 0x936c90d2, 0x616b5d22, 0x67b098e4,
};

/* 32-bit FNV1a constants */
#define FNV1A_OFFSET 0x811c9dc5
#define FNV1A_PRIME   0x1000193

static objtbl_hash_t hashfunc_prim(uintptr_t obj, objtbl_hash_t seed) {
  unsigned char *data = (unsigned char *)&obj;
  objtbl_hash_t hash = seed;
  int i;

  for (i = 0; i < sizeof(obj); i++) {
    hash = (hash ^ *data) * FNV1A_PRIME;
    data++;
  }

  return hash;
}

static int cmpfunc_prim(uintptr_t left, uintptr_t right) {
  return ((left > right) - (left < right));
}

static objtbl_hash_t hashfunc_testobj(uintptr_t obj, objtbl_hash_t seed) {
  unsigned char *data = (unsigned char *)&obj;
  objtbl_hash_t hash = seed;
  int i;

  for (i = 0; i < sizeof(obj); i++) {
    hash = (hash ^ *data) * FNV1A_PRIME;
    data++;
  }

  return hash;
}

static int cmpfunc_testobj(uintptr_t left, uintptr_t right) {
  const char *a = (const char *)left;
  const char *b = (const char *)right;

  return strcmp(a, b);
}

/* test up object hashing and comparator for testobj tables */
static void objopts(struct objtbl_opts *dst, struct objtbl_opts *src) {
  *dst = *src;
  dst->hashfunc = hashfunc_testobj;
  dst->cmpfunc = cmpfunc_testobj;
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

  ret = objtbl_init(&objtbl, &opts->tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&objtbl, 0);
  if (objtbl_contains(&objtbl, 0x8029au)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* insert a value in objtbl */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, 0x29au);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&objtbl, 1);
  if (objtbl_contains(&objtbl, 0x8029au)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the value of an existing element */
  ASSERT_SIZE(&objtbl, 1);
  if (!objtbl_contains(&objtbl, 0x29au)) {
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
static int test_insert_contains_nullval(struct opts *opts) {
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;

  ret = objtbl_init(&objtbl, &opts->tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&objtbl, 0);
  if (objtbl_contains(&objtbl, 0x8029au)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* insert a zero-value in objtbl */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, 0);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&objtbl, 1);
  if (objtbl_contains(&objtbl, 0x8000029au)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the value of an existing element */
  ASSERT_SIZE(&objtbl, 1);
  if (!objtbl_contains(&objtbl, 0)) {
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


/* test simple insertion and lookup */
static int test_insert_get(struct opts *opts) {
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;
  uintptr_t val = 0;

  ret = objtbl_init(&objtbl, &opts->tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_get(&objtbl, 0x8029au, &val);
  if (ret != OBJTBL_ENOTFOUND) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* insert a value in objtbl */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, 0x29au);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&objtbl, 1);
  ret = objtbl_get(&objtbl, 0x8029au, &val);
  if (ret != OBJTBL_ENOTFOUND) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the value of an existing element */
  ASSERT_SIZE(&objtbl, 1);
  ret = objtbl_get(&objtbl, 0x29au, &val);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* make sure we retrieve the correct value */
  ASSERT_SIZE(&objtbl, 1);
  if (val != 0x29au) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

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
  uintptr_t val = 0;

  /* initialize the table, with an "expected" element count of four */
  ret = objtbl_init(&objtbl, &opts->tblopts, 4);
  if (ret != OBJTBL_OK) {
    goto end;
  }

  /* insert a bunch of test values */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_insert(&objtbl, (uintptr_t)test_values[i]);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* retrieve and validate table content */
  ASSERT_SIZE(&objtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, test_values[i], &val);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }

    if (val != test_values[i]) {
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

/* test insertion and lookup, with table rehash and non-sequential IDs  */
static int test_insert_get_rehash_nonseq(struct opts *opts) {
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;
  uint32_t i;
  uint32_t ntest_values = sizeof(test_values) / sizeof(*test_values);
  uintptr_t val = 0;

  ret = objtbl_init(&objtbl, &opts->tblopts, 16);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* insert a bunch of test values */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_insert(&objtbl, (uintptr_t)test_values[i]);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* retrieve and validate table content */
  ASSERT_SIZE(&objtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, test_values[i], &val);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }

    if (val != test_values[i]) {
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
  uintptr_t val;
  int ret;

  ret = objtbl_init(&objtbl, &opts->tblopts, 8);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* retrieve non-existent content */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, test_values[i], &val);
    if (ret != OBJTBL_ENOTFOUND) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* insert table content */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_insert(&objtbl, (uintptr_t)test_values[i]);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* retrieve existing table content and validate result */
  ASSERT_SIZE(&objtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, test_values[i], &val);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }

    if (val != test_values[i]) {
      LOG_FAILURE();
      goto end_objtbl_cleanup;
    }
  }

  /* remove existing table content */
  ASSERT_SIZE(&objtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_remove(&objtbl, (uintptr_t)test_values[i]);
    if (ret != OBJTBL_OK) {
      objtbl_dump(&objtbl, stderr);
      LOG_FAILUREF("index:%u", i);
      goto end_objtbl_cleanup;
    }
  }

  /* retrieve non-existent content */
  ASSERT_SIZE(&objtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = objtbl_get(&objtbl, test_values[i], &val);
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

/* test simple insertion and lookup using contains */
static int test_obj_insert(struct opts *opts) {
  struct objtbl_ctx objtbl;
  struct objtbl_opts tblopts;
  int result = EXIT_FAILURE;
  int ret;
  struct testobj noexist = {.key = "wololo", .value = 2};
  struct testobj exist   = {.key = "iexist", .value = 0x29a};

  objopts(&tblopts, &opts->tblopts);
  ret = objtbl_init(&objtbl, &tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&objtbl, 0);
  if (objtbl_contains(&objtbl, (uintptr_t)&noexist)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* insert a value in objtbl */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, (uintptr_t)&exist);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&objtbl, 1);
  if (objtbl_contains(&objtbl, (uintptr_t)&noexist)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the value of an existing element */
  ASSERT_SIZE(&objtbl, 1);
  if (!objtbl_contains(&objtbl, (uintptr_t)&exist)) {
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
static int test_obj_insert_duplicate(struct opts *opts) {
  struct objtbl_ctx objtbl;
  struct objtbl_opts tblopts;
  int result = EXIT_FAILURE;
  int ret;
  struct testobj valA   = {.key = "wololo", .value = 2};
  struct testobj valB   = {.key = "wololo", .value = 0x29a};
  struct testobj keyobj = {.key = "wololo"};
  uintptr_t resptr = 0;
  struct testobj *res;

  objopts(&tblopts, &opts->tblopts);
  ret = objtbl_init(&objtbl, &tblopts, 1);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* insert a value in objtbl */
  ASSERT_SIZE(&objtbl, 0);
  ret = objtbl_insert(&objtbl, (uintptr_t)&valA);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* check existance of inserted element */
  ASSERT_SIZE(&objtbl, 1);
  if (!objtbl_contains(&objtbl, (uintptr_t)&valA)) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the inserted element */
  ASSERT_SIZE(&objtbl, 1);
  ret = objtbl_get(&objtbl, (uintptr_t)&keyobj, &resptr);
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
  ret = objtbl_insert(&objtbl, (uintptr_t)&valB);
  if (ret != OBJTBL_OK) {
    LOG_FAILURE();
    goto end_objtbl_cleanup;
  }

  /* retrieve the inserted element, now expected to have replaced the
   * previously inserted element  */
  resptr = 0;
  ASSERT_SIZE(&objtbl, 1);
  ret = objtbl_get(&objtbl, (uintptr_t)&keyobj, &resptr);
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

static int test_stats(struct opts *opts) {
  struct objtbl_stats stats;
  struct objtbl_ctx objtbl;
  int result = EXIT_FAILURE;
  int ret;
  size_t i;
  size_t nelems;

  srand((unsigned int)opts->tblopts.hashseed);
  for (nelems = 100;  nelems <= 100000; nelems *= 10) {
    ret = objtbl_init(&objtbl, &opts->tblopts, 8);
    if (ret != OBJTBL_OK) {
      LOG_FAILURE();
      goto end;
    }

    for (i = 0; i < nelems; i++) {
      ret = objtbl_insert(&objtbl, (uintptr_t)rand());
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
      ret = objtbl_insert(&objtbl, (uintptr_t)i);
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
  return result;
end_objtbl_cleanup:
  objtbl_cleanup(&objtbl);
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
  /* default to primitive data types - overridable by single tests using
   * objopts */
  opts->tblopts.hashfunc = hashfunc_prim;
  opts->tblopts.cmpfunc = cmpfunc_prim;

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
    {"insert_contains", test_insert_contains},
    {"insert_contains_nullval", test_insert_contains_nullval},
    {"insert_get", test_insert_get},
    {"insert_get_rehash", test_insert_get_rehash},
    {"insert_get_rehash_nonseq", test_insert_get_rehash_nonseq},
    {"get_insert_get_remove_get", test_get_insert_get_remove_get},
    {"obj_insert", test_obj_insert},
    {"obj_insert_duplicate", test_obj_insert_duplicate},
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
