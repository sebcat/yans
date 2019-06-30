#include <string.h>

#include <lib/util/vaguever.h>
#include <lib/util/macros.h>
#include <lib/util/test.h>

static int test_compare() {
  int result = TEST_OK;
  int val;
  size_t i;
  struct vaguever_version v1;
  struct vaguever_version v2;
  struct {
    const char *v1;
    const char *v2;
    int expected;
  } tests[] = {
    {"1.0", "1.0", 0},
    {"1.0", "1", 0},
    {"1", "1.0", 0},
    {"1", "1", 0},
    {"1.1", "1.0", 1},
    {"1.1.1", "1.1", 1},
    {"1.1", "1.0.1", 1},
    {"1.0", "1.1", -1},
    {"1.1", "1.1.1", -1},
    {"1.0.1", "1.1", -1},
  };

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    vaguever_init(&v1, tests[i].v1);
    vaguever_init(&v2, tests[i].v2);
    val = vaguever_cmp(&v1, &v2);
    if (val != tests[i].expected) {
      TEST_LOGF("entry %zu: expected %d, was: %d", i, tests[i].expected,
          val);
      result = TEST_FAIL;
    }
  }

  return result;
}

int test_parse() {
  size_t i;
  struct {
    const char *input;
    const char *expected;
  } tests[] = {
    {"", "0"},
    {"0", "0"},
    {"00", "0"},
    {"01", "1"},
    {"10", "10"},
    {"11", "11"},
    {"0.", "0"},
    {"0.1", "0.1"},
    {"0.1.", "0.1"},
    {"0.1.2", "0.1.2"},
    {"0.1.2.", "0.1.2"},
    {"0.1.2.3", "0.1.2.3"},
    {"0.1.2.3.", "0.1.2.3"},
    {"0.1.2.3.4", "0.1.2.3"},
    {"7.2p666", "7.2"}, /* FYI regarding vaguever OpenSSH behavior */
    {"1.0.1", "1.0.1"},
    {"0.1.0", "0.1.0"},
    {"0.1.0", "0.1.0"},
    {"0..1", "0.1"},
    {"0...1", "0.1"},
  };
  struct vaguever_version v;
  char buf[64];
  int result = TEST_OK;

  for (i = 0; i < ARRAY_SIZE(tests); i++) {
    vaguever_init(&v, tests[i].input);
    vaguever_str(&v, buf, sizeof(buf));
    if (strcmp(tests[i].expected, buf) != 0) {
      TEST_LOGF("input:\"%s\" expected:\"%s\", actual:\"%s\"\n",
          tests[i].input, tests[i].expected, buf);
      result = TEST_FAIL;
    }
  }

  return result;
}

TEST_ENTRY(
  {"parse", test_parse},
  {"compare", test_compare},
);
