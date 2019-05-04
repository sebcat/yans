#include <string.h>

#include <lib/match/tcpproto.h>
#include <lib/util/test.h>

static int test_init_cleanup() {
  struct tcpproto_ctx ctx;
  int ret;

  ret = tcpproto_init(&ctx);
  if (ret != 0) {
    TEST_LOG("tcpproto_init failure");
    return TEST_FAIL;
  }

  tcpproto_cleanup(&ctx);
  return TEST_OK;
}

static int test_type_to_string() {
  const char *str;
  int i;
  static const struct {
    enum tcpproto_type proto;
    const char *str;
  } tests[] = {
    {TCPPROTO_SSH, "ssh"},
    {TCPPROTO_SMTP, "smtp"},
    {TCPPROTO_SMTPS, "smtps"},
    {TCPPROTO_UNKNOWN, "unknown"},
    {NBR_OF_TCPPROTOS + 4, "unknown"},
    {0, NULL},
  };

  for (i = 0; tests[i].str != NULL; i++) {
    str = tcpproto_type_to_string(tests[i].proto);
    if (str == NULL) {
      TEST_LOGF("failed to get string representation (index:%d)", i);
      return TEST_FAIL;
    }

    if (strcmp(str, tests[i].str) != 0) {
      TEST_LOGF("expected \"%s\", got \"%s\" (index:%d)",
          tests[i].str, str, i);
      return TEST_FAIL;
    }
  }

  return TEST_OK;
}

static int test_type_from_port() {
  enum tcpproto_type t;
  size_t i;
  static const struct {
    unsigned short port;
    enum tcpproto_type proto;
  } tests[] = {
    {22, TCPPROTO_SSH},
    {0, TCPPROTO_UNKNOWN},
    {65535, TCPPROTO_UNKNOWN},
    {80, TCPPROTO_HTTP},
    {443, TCPPROTO_HTTPS},
  };

  for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
    t = tcpproto_type_from_port(tests[i].port);
    if (t != tests[i].proto) {
      TEST_LOGF("expected:%d got:%d index:%zu", tests[i].proto, t, i);
      return TEST_FAIL;
    }
  }

  return TEST_OK;
}

static int test_match() {
  enum tcpproto_type t;
  struct tcpproto_ctx ctx;
  int ret;
  int result = TEST_OK;
  size_t i;
  static const struct {
    const char *data;
    enum tcpproto_type proto_plain;
    enum tcpproto_type proto_tls;
  } tests[] = {
    {"", TCPPROTO_UNKNOWN, TCPPROTO_UNKNOWN},
    {"220 mail.example.com ESMTP Sat, 12 Jan 2019 21:00:09 +0100",
        TCPPROTO_SMTP, TCPPROTO_SMTPS},
    {"220 Please use http://ftp.acc.umu.se/ whenever possible.",
        TCPPROTO_FTP, TCPPROTO_FTPS},
    {"220---------- Welcome to Pure-FTPd [privsep] [TLS] ----------",
        TCPPROTO_FTP, TCPPROTO_FTPS},
    {"+OK mail.example.com POP3 ready <foo@mail.example.com>",
        TCPPROTO_POP3, TCPPROTO_POP3S},
    {"* OK mail.example.com IMAP4rev1 ready",
        TCPPROTO_IMAP, TCPPROTO_IMAPS},
    {"NOTICE AUTH :*** Processing connection to irc.example.com",
        TCPPROTO_IRC, TCPPROTO_IRCS},
    {"SSH-2.0-OpenSSH_6.7p1 Debian-5+deb8u7",
        TCPPROTO_SSH, TCPPROTO_SSH},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nlel\n",
        TCPPROTO_HTTP, TCPPROTO_HTTPS},
    {"HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nlel\n",
        TCPPROTO_HTTP, TCPPROTO_HTTPS},
  };

  ret = tcpproto_init(&ctx);
  if (ret != 0) {
    TEST_LOG("tcpproto_init failure");
    return TEST_FAIL;
  }

  for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
    t = tcpproto_match(&ctx, tests[i].data, strlen(tests[i].data), 0);
    if (t != tests[i].proto_plain) {
      TEST_LOGF("tcpproto_match plain expected:%d got:%d index:%zu",
          tests[i].proto_plain, t, i);
      result = TEST_FAIL;
    }

    t = tcpproto_match(&ctx, tests[i].data, strlen(tests[i].data),
        TCPPROTO_MATCHF_TLS);
    if (t != tests[i].proto_tls) {
      TEST_LOGF("tcpproto_match tls expected:%d got:%d index:%zu",
          tests[i].proto_plain, t, i);
      result = TEST_FAIL;
    }
  }

  tcpproto_cleanup(&ctx);
  return result;
}

TEST_ENTRY(
  {"init_cleanup", test_init_cleanup},
  {"type_to_string", test_type_to_string},
  {"type_from_port", test_type_from_port},
  {"match", test_match},
);
