#include <lib/util/idset.h>
#include <lib/util/test.h>

static int test_instantiation() {
  struct idset_ctx *set;

  set = idset_new(10);
  if (set == NULL) {
    TEST_LOG("failed to instantiate set: nids 10");
    return TEST_FAIL;
  }

  idset_free(set);
  set = idset_new(0);
  if (set != NULL) {
    TEST_LOG("failed to instantiate set: nids 0");
    return TEST_FAIL;
  }

  idset_free(set);
  return TEST_OK;
}

static int test_interval() {
  struct idset_ctx *set;
  unsigned int nids[] = {1, 2, 65};
  const int max_iter = 100;
  int i;
  int j;
  int id;
  
  for (i = 0; i < sizeof(nids)/sizeof(*nids); i++) {
    set = idset_new(nids[i]);
    if (set == NULL) {
      TEST_LOGF("failed to instantiate set: nids: %d", nids[i]);
      return TEST_FAIL;
    }

    for (j = 0; j < max_iter; j++) {
      id = idset_use_next(set);
      if (id == -1) {
        break;
      } else if (id != j) {
        TEST_LOGF("i:%d - expected id:%d, got:%d", i, j, id);
        goto fail;
      }
    }

    if (j != nids[i]) {
      TEST_LOGF("i:%d - expected end:%d, got:%d", i, nids[i], j);
      goto fail;
    }

    idset_free(set);
  }

  return TEST_OK;
fail:
  idset_free(set);
  return TEST_FAIL;
}

static int test_reuse() {
  int result = TEST_FAIL;
  struct idset_ctx *set;
  int id;

  set = idset_new(2);
  if (set == NULL) {
    TEST_LOG("failed to instantiate set");
    return TEST_FAIL;
  }

  id = idset_use_next(set);
  if (id != 0) {
    TEST_LOGF("expected 0, got %d", id);
    goto done;
  }

  id = idset_use_next(set);
  if (id != 1) {
    TEST_LOGF("expected 1, got %d", id);
    goto done;
  }

  id = idset_use_next(set);
  if (id != -1) {
    TEST_LOGF("expected -1, got %d", id);
    goto done;
  }

  idset_clear(set, 1);

  id = idset_use_next(set);
  if (id != 1) {
    TEST_LOGF("expected 1, got %d", id);
    goto done;
  }

  id = idset_use_next(set);
  if (id != -1) {
    TEST_LOGF("expected -1, got %d", id);
    goto done;
  }

  idset_clear(set, 0);

  id = idset_use_next(set);
  if (id != 0) {
    TEST_LOGF("expected 0, got %d", id);
    goto done;
  }

  id = idset_use_next(set);
  if (id != -1) {
    TEST_LOGF("expected -1, got %d", id);
    goto done;
  }

  result = TEST_OK;
done:
  idset_free(set);
  return result;
}

TEST_ENTRY(
    {"instantiation", test_instantiation},
    {"interval",      test_interval},
    {"reuse",         test_reuse},
);
