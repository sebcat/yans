#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <stdint.h>

#include <lib/util/idtbl.h>

#define LOG_FAILURE() \
    fprintf(stderr, "%s:%d test failure\n", __FILE__, __LINE__)

/* A little bit dirty to have a goto inside a macro like this, but it will
 * do for now... */
#define ASSERT_SIZE(tbl_, size_)           \
    do {                                   \
      if (idtbl_size((tbl_)) != (size_)) { \
        LOG_FAILURE();                     \
        goto end_idtbl_cleanup;            \
      }                                    \
    } while(0)

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

struct opts {
  uint32_t seed;
  int dump_tables;
};

/* test initialization and cleanup, with varying # of slots and seed  */
static int test_init_cleanup(struct opts *opts) {
  uint32_t i;
  struct idtbl_ctx idtbl;
  int ret;

  for (i = 0; i < 10; i++) {
    ret = idtbl_init(&idtbl, i, i * 3);
    idtbl_cleanup(&idtbl);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      return EXIT_FAILURE;
    }
  }

  for (i = 0; i < 10; i++) {
    ret = idtbl_init(&idtbl, i*3, i);
    idtbl_cleanup(&idtbl);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

/* test simple insertion and lookup using contains */
static int test_insert_contains(struct opts *opts) {
  struct idtbl_ctx idtbl;
  int result = EXIT_FAILURE;
  int ret;

  ret = idtbl_init(&idtbl, 1, opts->seed);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&idtbl, 0);
  if (idtbl_contains(&idtbl, 0x8029au)) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* insert a value in idtbl */
  ASSERT_SIZE(&idtbl, 0);
  ret = idtbl_insert(&idtbl, 0x29au, &ret);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&idtbl, 1);
  if (idtbl_contains(&idtbl, 0x8029au)) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* retrieve the value of an existing element */
  ASSERT_SIZE(&idtbl, 1);
  if (!idtbl_contains(&idtbl, 0x29au)) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  ASSERT_SIZE(&idtbl, 1);
  result = EXIT_SUCCESS;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
end:
  return result;
}

/* test simple insertion and lookup using contains, with a NULL value */
static int test_insert_contains_nullval(struct opts *opts) {
  struct idtbl_ctx idtbl;
  int result = EXIT_FAILURE;
  int ret;

  ret = idtbl_init(&idtbl, 1, opts->seed);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&idtbl, 0);
  if (idtbl_contains(&idtbl, 0x8029au)) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* insert a NULL value in idtbl */
  ASSERT_SIZE(&idtbl, 0);
  ret = idtbl_insert(&idtbl, 0x29au, NULL);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&idtbl, 1);
  if (idtbl_contains(&idtbl, 0x8000029au)) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* retrieve the value of an existing element */
  ASSERT_SIZE(&idtbl, 1);
  if (!idtbl_contains(&idtbl, 0x29au)) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  ASSERT_SIZE(&idtbl, 1);
  result = EXIT_SUCCESS;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
end:
  return result;
}


/* test simple insertion and lookup */
static int test_insert_get(struct opts *opts) {
  struct idtbl_ctx idtbl;
  int result = EXIT_FAILURE;
  int ret;
  void *val = NULL;

  ret = idtbl_init(&idtbl, 1, opts->seed);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* test non-existent retrieval in empty table */
  ASSERT_SIZE(&idtbl, 0);
  ret = idtbl_get(&idtbl, 0x8029au, &val);
  if (ret != IDTBL_ENOTFOUND) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* insert a value in idtbl */
  ASSERT_SIZE(&idtbl, 0);
  ret = idtbl_insert(&idtbl, 0x29au, &val);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* test non-existent retrieval in non-empty table */
  ASSERT_SIZE(&idtbl, 1);
  ret = idtbl_get(&idtbl, 0x8029au, &val);
  if (ret != IDTBL_ENOTFOUND) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* retrieve the value of an existing element */
  ASSERT_SIZE(&idtbl, 1);
  ret = idtbl_get(&idtbl, 0x29au, &val);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* make sure we retrieve the correct value */
  ASSERT_SIZE(&idtbl, 1);
  if (val != &val) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  result = EXIT_SUCCESS;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
end:
  return result;
}

/* test insertion w/ duplicate key */
static int test_insert_get_duplicate(struct opts *opts) {
  struct idtbl_ctx idtbl;
  int result = EXIT_FAILURE;
  int ret;
  int val1 = 22;
  int val2 = 33;
  void *val = NULL;

  ret = idtbl_init(&idtbl, 1, opts->seed);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* insert a value in idtbl */
  ASSERT_SIZE(&idtbl, 0);
  ret = idtbl_insert(&idtbl, 0x29au, &val1);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  /* insert a value in idtbl with the same key as the previous one */
  ASSERT_SIZE(&idtbl, 1);
  ret = idtbl_insert(&idtbl, 0x29au, &val2);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  ASSERT_SIZE(&idtbl, 1);
  ret = idtbl_get(&idtbl, 0x29au, &val);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  ASSERT_SIZE(&idtbl, 1);
  if (*(int*)val != val2) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  ASSERT_SIZE(&idtbl, 1);
  if (idtbl_size(&idtbl) != 1) {
    LOG_FAILURE();
    goto end_idtbl_cleanup;
  }

  result = EXIT_SUCCESS;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
end:
  return result;
}

/* test insertion and lookup, with table rehash */
static int test_insert_get_rehash(struct opts *opts) {
  struct idtbl_ctx idtbl;
  int result = EXIT_FAILURE;
  int ret;
  uint32_t i;
  uint32_t ntest_values = sizeof(test_values) / sizeof(*test_values);
  void *val = NULL;

  /* initialize the table, with an "expected" element count of four */
  ret = idtbl_init(&idtbl, 4, opts->seed);
  if (ret != IDTBL_OK) {
    goto end;
  }

  /* insert pointers to a bunch of test values */
  ASSERT_SIZE(&idtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_insert(&idtbl, i, test_values + i);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  /* retrieve and validate table content */
  ASSERT_SIZE(&idtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_get(&idtbl, i, &val);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }

    if (*(uint32_t*)val != test_values[i]) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  result = EXIT_SUCCESS;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
end:
  return result;
}

/* test insertion and lookup, with table rehash and non-sequential IDs  */
static int test_insert_get_rehash_nonseq(struct opts *opts) {
  struct idtbl_ctx idtbl;
  int result = EXIT_FAILURE;
  int ret;
  uint32_t i;
  uint32_t ntest_values = sizeof(test_values) / sizeof(*test_values);
  void *val = NULL;

  ret = idtbl_init(&idtbl, 16, opts->seed);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* insert a bunch of test values */
  ASSERT_SIZE(&idtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_insert(&idtbl, test_values[i], test_values + i);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  /* retrieve and validate table content */
  ASSERT_SIZE(&idtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_get(&idtbl, test_values[i], &val);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }

    if (*(uint32_t*)val != test_values[i]) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  result = EXIT_SUCCESS;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
end:
  return result;
}

static int test_get_insert_get_remove_get(struct opts *opts) {
  struct idtbl_ctx idtbl;
  uint32_t i;
  uint32_t ntest_values = sizeof(test_values) / sizeof(*test_values);
  int result = EXIT_FAILURE;
  void *val;
  int ret;

  ret = idtbl_init(&idtbl, 8, opts->seed);
  if (ret != IDTBL_OK) {
    LOG_FAILURE();
    goto end;
  }

  /* retrieve non-existent content */
  ASSERT_SIZE(&idtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_get(&idtbl, test_values[i], &val);
    if (ret != IDTBL_ENOTFOUND) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  /* insert table content */
  ASSERT_SIZE(&idtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_insert(&idtbl, test_values[i], test_values + i);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  /* retrieve existing table content and validate result */
  ASSERT_SIZE(&idtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_get(&idtbl, test_values[i], &val);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }

    if (*(uint32_t*)val != test_values[i]) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  /* remove existing table content */
  ASSERT_SIZE(&idtbl, ntest_values);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_remove(&idtbl, test_values[i]);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  /* retrieve non-existent content */
  ASSERT_SIZE(&idtbl, 0);
  for (i = 0; i < ntest_values; i++) {
    ret = idtbl_get(&idtbl, test_values[i], &val);
    if (ret != IDTBL_ENOTFOUND) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }
  }

  result = EXIT_SUCCESS;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
end:
  return result;
}

static void dump_table(struct idtbl_ctx *ctx, FILE *fp) {
  struct idtbl_entry *ent;
  uint32_t i;

  for (i = 0; i < ctx->header.cap; i++) {
    ent = ctx->entries + i;
    fprintf(fp, "      %s distance:%u\n", ent->key ? "XXXX" : "____",
      ent->distance);
  }

}

static int test_stats(struct opts *opts) {
  struct idtbl_stats stats;
  struct idtbl_ctx idtbl;
  int result = EXIT_FAILURE;
  int ret;
  size_t i;
  size_t nelems;

  srand((unsigned int)opts->seed);
  for (nelems = 100;  nelems <= 100000; nelems *= 10) {
    ret = idtbl_init(&idtbl, 8, opts->seed);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end;
    }

    for (i = 0; i < nelems; i++) {
      ret = idtbl_insert(&idtbl, (uint32_t)rand(), NULL);
      if (ret != IDTBL_OK) {
        LOG_FAILURE();
        goto end_idtbl_cleanup;
      }
    }

    ret = idtbl_calc_stats(&idtbl, &stats);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }

    printf("    rnd-%zu: nbytes:%zu load:%.2f avg:%.2f mean:%u max:%u\n"
        , nelems, stats.nbytes, (double)stats.size / (double)stats.cap,
        stats.average_probe_distance, stats.mean_probe_distance,
        stats.max_probe_distance);
    if (opts->dump_tables) {
      dump_table(&idtbl, stdout);
    }

    idtbl_cleanup(&idtbl);

    ret = idtbl_init(&idtbl, 8, opts->seed);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end;
    }

    for (i = 0; i < nelems; i++) {
      ret = idtbl_insert(&idtbl, (uint32_t)i, NULL);
      if (ret != IDTBL_OK) {
        LOG_FAILURE();
        goto end_idtbl_cleanup;
      }
    }

    ret = idtbl_calc_stats(&idtbl, &stats);
    if (ret != IDTBL_OK) {
      LOG_FAILURE();
      goto end_idtbl_cleanup;
    }

    printf("    seq-%zu: nbytes:%zu load:%.2f avg:%.2f mean:%u max:%u\n"
        , nelems, stats.nbytes, (double)stats.size / (double)stats.cap,
        stats.average_probe_distance, stats.mean_probe_distance,
        stats.max_probe_distance);
    if (opts->dump_tables) {
      dump_table(&idtbl, stdout);
    }

    idtbl_cleanup(&idtbl);
  }
  
  result = EXIT_SUCCESS;
end:
  return result;
end_idtbl_cleanup:
  idtbl_cleanup(&idtbl);
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
  opts->seed = tp.tv_nsec;
  opts->dump_tables = 0;

  while ((ch = getopt_long(argc, argv, optstr, lopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      opts->seed = (uint32_t)strtoul(optarg, NULL, 10);
      break;
    case 'd':
      opts->dump_tables = 1;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

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
  struct opts opts;
  struct {
    const char *name;
    int (*func)(struct opts *);
  } tests[] = {
    {"init_cleanup", test_init_cleanup},
    {"insert_contains", test_insert_contains},
    {"insert_contains_nullval", test_insert_contains_nullval},
    {"insert_get", test_insert_get},
    {"insert_get_duplicate", test_insert_get_duplicate},
    {"insert_get_rehash", test_insert_get_rehash},
    {"insert_get_rehash_nonseq", test_insert_get_rehash_nonseq},
    {"get_insert_get_remove_get", test_get_insert_get_remove_get},
    {"stats", test_stats},
  };

  /* load and print options */
  opts_or_die(&opts, argc, argv);
  printf("    seed:%u\n", opts.seed);

  for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
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
