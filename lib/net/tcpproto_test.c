#include <string.h>

#include <lib/net/tcpproto.h>
#include <lib/util/test.h>

static int test_init_cleanup() {
  struct tcpproto_ctx ctx;
  int ret;

  ret = tcpproto_init(&ctx);
  if (ret != 0) {
    TEST_LOG_ERR("tcpproto_init failure");
    return TEST_FAIL;
  }

  tcpproto_cleanup(&ctx);
  return TEST_OK;
}

static int test_type_to_string() {
  const char *str;

  str = tcpproto_type_to_string(TCPPROTO_SSH);
  if (str == NULL) {
    TEST_LOG_ERR("unable to obtain string representation of SSH");
    return TEST_FAIL;
  }

  if (strcmp(str, "ssh") != 0) {
   TEST_LOG_ERRF("expected \"ssh\", got \"%s\"", str);
   return TEST_FAIL;
  }

  return TEST_OK;
}

static int test_type_from_port() {
  enum tcpproto_type t;

  t = tcpproto_type_from_port(22);
  if (t != TCPPROTO_SSH) {
    TEST_LOG_ERRF("expected TCPPROTO_SSH (%d), got: %d", TCPPROTO_SSH, t);
    return TEST_FAIL;
  }

  return TEST_OK;
}
/*
static int test_() {
  return TEST_FAIL;
}
*/
TEST_ENTRY(
  {"init_cleanup", test_init_cleanup},
  {"type_to_string", test_type_to_string},
  {"type_from_port", test_type_from_port},
);
