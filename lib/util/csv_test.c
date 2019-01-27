#include <string.h>

#include <lib/util/csv.h>
#include <lib/util/test.h>

#define NELEMS_MAX 16 /* maximum number of test strings per entry */

static int test_encode() {
  const char **cptr;
  int result = TEST_OK;
  int ret;
  buf_t buf;
  size_t i;
  static const struct {
    char *elems[NELEMS_MAX];
    size_t nelems;
    const char *expected;
  } vals[] = {
    {{NULL}, 0, ""},
    {{""}, 1, "\r\n"},
    {{"x"}, 1, "x\r\n"},
    {{"xy"}, 1, "xy\r\n"},
    {{"xyz"}, 1, "xyz\r\n"},
    {{"\""}, 1, "\"\"\"\"\r\n"},
    {{"\"\""}, 1, "\"\"\"\"\"\"\r\n"},
    {{"\"\"\""}, 1, "\"\"\"\"\"\"\"\"\r\n"},
    {{"", ""}, 2, ",\r\n"},
    {{"", "y"}, 2, ",y\r\n"},
    {{"x", ""}, 2, "x,\r\n"},
    {{"x", "y"}, 2, "x,y\r\n"},
    {{"", "yy"}, 2, ",yy\r\n"},
    {{"xx", ""}, 2, "xx,\r\n"},
    {{"xx", "yy"}, 2, "xx,yy\r\n"},
    {{"", "yyy"}, 2, ",yyy\r\n"},
    {{"xxx", ""}, 2, "xxx,\r\n"},
    {{"xxx", "yyy"}, 2, "xxx,yyy\r\n"},
    {{"foo", "bar\r\nbaz", "foobar"}, 3, "foo,\"bar\r\nbaz\",foobar\r\n"}, 
    {{"foo", "bar\r\n\"\r\nbaz", "foobar"}, 3,
        "foo,\"bar\r\n\"\"\r\nbaz\",foobar\r\n"}, 
  };

  buf_init(&buf, 32);
  for (i = 0; i < sizeof(vals) / sizeof(*vals); i++) {
    buf_clear(&buf);
    cptr = (const char **)vals[i].elems;
    ret = csv_encode(&buf, cptr, vals[i].nelems);
    if (ret != 0) {
      TEST_LOG_ERRF("index:%zu csv_encode failure", i);
      result = TEST_FAIL;
    }

    if (strncmp(buf.data, vals[i].expected, buf.len) != 0) {
      buf_achar(&buf, '\0'); /* \0-terminate for the log message */
      TEST_LOG_ERRF("index:%zu expected:%s was:%s", i, vals[i].expected,
          buf.data);
      result = TEST_FAIL;
    }
  }

  buf_cleanup(&buf);
  return result;
}

TEST_ENTRY(
  {"encode", test_encode},
);
