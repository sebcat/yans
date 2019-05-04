#include <lib/match/pm.h>
#include <lib/util/test.h>

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

TEST_ENTRY(
  {"init_cleanup", test_init_cleanup},
);
