#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lib/match/pm.h>
#include <lib/util/test.h>

#define CSV_TEST_FILE "./data/pm/1.pm"
#define CSV_MIN_ROWS 18 /* subject to change, update as needed */

static int test_init_cleanup() {
  int ret;
  struct pm_ctx pm;

  ret = pm_init(&pm);
  if (ret != 0) {
    TEST_LOGF("pm_init failure (%d)", ret);
    return TEST_FAIL;
  }

  pm_cleanup(&pm);
  return TEST_OK;
}

static int test_load_csv() {
  int ret;
  int status = TEST_FAIL;
  struct pm_ctx pm;
  size_t npatterns;
  FILE *fp;

  ret = pm_init(&pm);
  if (ret != 0) {
    TEST_LOGF("pm_init failure (%d)", ret);
    return TEST_FAIL;
  }

  fp = fopen(CSV_TEST_FILE, "rb");
  if (!fp) {
    TEST_LOGF(CSV_TEST_FILE ": %s", strerror(errno));
    goto done;
  }

  ret = pm_load_csv(&pm, fp, &npatterns);
  if (ret != 0) {
    TEST_LOG("failed to load patterns from CSV");
    goto done;
  }

  if (npatterns < CSV_MIN_ROWS) {
    TEST_LOGF("number of rows, min:%zu actual:%zu\n", (size_t)CSV_MIN_ROWS,
        npatterns);
    goto done;
  }

  ret = pm_compile(&pm);
  if (ret != 0) {
    TEST_LOG("failed to compile loaded patterns");
    goto done;
  }

  status = TEST_OK;
done:
  pm_cleanup(&pm);
  return status;
}

TEST_ENTRY(
  {"init_cleanup", test_init_cleanup},
  {"load_csv", test_load_csv},
);
