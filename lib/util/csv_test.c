#include <string.h>

#include <lib/util/csv.h>
#include <lib/util/test.h>
#include <lib/util/macros.h>

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
  for (i = 0; i < ARRAY_SIZE(vals); i++) {
    buf_clear(&buf);
    cptr = (const char **)vals[i].elems;
    ret = csv_encode(&buf, cptr, vals[i].nelems);
    if (ret != 0) {
      TEST_LOGF("index:%zu csv_encode failure", i);
      result = TEST_FAIL;
    }

    if (strncmp(buf.data, vals[i].expected, buf.len) != 0) {
      buf_achar(&buf, '\0'); /* \0-terminate for the log message */
      TEST_LOGF("index:%zu expected:%s was:%s", i, vals[i].expected,
          buf.data);
      result = TEST_FAIL;
    }
  }

  buf_cleanup(&buf);
  return result;
}

static int test_read_row() {
  const char *col;
  size_t nelems;
  int result = TEST_OK;
  size_t i;
  size_t j;
  FILE *fp;
  struct csv_reader r;
  int ret;
  static const struct {
    char *input;
    size_t nelems;
    char *elems[NELEMS_MAX];
  } vals[] = {
    {"foo", 1, {"foo"}},
    {",", 2, {"", ""}},
    {",,", 3, {"", "", ""}},
    {"foo,", 2, {"foo", ""}},
    {",foo", 2, {"", "foo"}},
    {"foo,bar", 2, {"foo", "bar"}},
    {"foo,bar\r", 2, {"foo", "bar"}},
    {"foo,bar\n", 2, {"foo", "bar"}},
    {"foo,bar\r\n", 2, {"foo", "bar"}},
    {",foo,bar", 3, {"", "foo", "bar"}},
    {"foo,bar,", 3, {"foo", "bar", ""}},
    {"foo,bar,\r\n", 3, {"foo", "bar", ""}},
    {",foo,bar,\r\n", 4, {"", "foo", "bar", ""}},

    {"\"foo\"", 1, {"foo"}},
    {",\"\"", 2, {"", ""}},
    {"\"\",", 2, {"", ""}},
    {"\"\",\"\"", 2, {"", ""}},
    {"\"foo\",", 2, {"foo", ""}},
    {",\"foo\"", 2, {"", "foo"}},
    {"\"foo\",\"bar\"", 2, {"foo", "bar"}},
    {"\"foo\",\"bar\"\r", 2, {"foo", "bar"}},
    {"\"foo\",\"bar\"\n", 2, {"foo", "bar"}},
    {"\"foo\",\"bar\"\r\n", 2, {"foo", "bar"}},
    {",\"foo\",\"bar\"", 3, {"", "foo", "bar"}},
    {"\"\",\"foo\",\"bar\"", 3, {"", "foo", "bar"}},
    {"\"foo\",\"bar\",\"\"", 3, {"foo", "bar", ""}},
    {"\"foo\",\"bar\",", 3, {"foo", "bar", ""}},
    {"\"foo\",\"bar\",\r\n", 3, {"foo", "bar", ""}},
    {"\"foo\",\"bar\",\"\"\r\n", 3, {"foo", "bar", ""}},
    {",\"foo\",\"bar\",\r\n", 4, {"", "foo", "bar", ""}},
    {"\"\",foo,bar,\r\n", 4, {"", "foo", "bar", ""}},
    {",foo,bar,\"\"\r\n", 4, {"", "foo", "bar", ""}},
    {"foo,\"foo\r\nbar\r\nbaz\"", 2, {"foo", "foo\r\nbar\r\nbaz"}},
    {"foo,\"foo\r\nbar\r\nbaz\",", 3, {"foo", "foo\r\nbar\r\nbaz", ""}},

    {"\"\"\"\"", 1, {"\""}},
    {"\"\"\"\",bar", 2, {"\"", "bar"}},

    /* invalid csvs, but we follow Postel here */
    {"\"\"\"", 1, {"\""}},
    {"\"foo", 1, {"foo"}},
    {"\"foo\"bar", 1, {"foobar"}},
    {"\"foo\"bar,baz", 1, {"foobar,baz"}},
  };

  csv_reader_init(&r);
  for (i = 0; i < ARRAY_SIZE(vals); i++) {
    fp = fmemopen(vals[i].input, strlen(vals[i].input), "rb");
    if (!fp) {
      TEST_LOGF("index:%zu failed to open input", i);
      result = TEST_FAIL;
      continue;
    }

    ret = csv_read_row(&r, fp);
    if (ret < 0) {
      TEST_LOGF("index:%zu failed to read csv input", i);
      result = TEST_FAIL;
    }

    fclose(fp);
    nelems = csv_reader_nelems(&r);
    if (nelems != vals[i].nelems) {
      TEST_LOGF("index:%zu nelems expected:%zu actual:%zu input:%s", i,
          vals[i].nelems, nelems, vals[i].input);
      result = TEST_FAIL;
    }

    nelems = MIN(nelems, vals[i].nelems);
    for (j = 0; j < nelems; j++) {
      col = csv_reader_elem(&r, j);
      if (strcmp(vals[i].elems[j], col) != 0) {
        TEST_LOGF("index:%zu elem:%zu expected:%s actual:%s input:%s",
            i, j, vals[i].elems[j], col, vals[i].input);
        result = TEST_FAIL;
      }
    }
  }

  csv_reader_cleanup(&r);

  return result;
}

TEST_ENTRY(
  {"encode", test_encode},
  {"read_row", test_read_row},
);
