#include <string.h>
#include <lib/util/csv.h>

static int csv_encode_escaped(buf_t *buf, const char *s) {
  int ret;

  ret = buf_achar(buf, '"');
  if (ret < 0) {
    return -1;
  }

  while (*s) {
    if (*s == '"') {
      ret = buf_adata(buf, "\"\"", 2);
      if (ret < 0) {
        return -1;
      }
    } else {
      buf_achar(buf, *s);
    }

    s++;
  }

  ret = buf_achar(buf, '"');
  if (ret < 0) {
    return -1;
  }

  return 0;
}

int csv_encode(buf_t *dst, const char **cols, size_t ncols) {
  int ret;
  size_t i;
  size_t off;

  for (i = 0; i < ncols; i++) {
    /* Emit field separator unless this is the first entry */
    if (i != 0) {
      ret = buf_achar(dst, ',');
      if (ret < 0) {
        return -1;
      }
    }

    /* check if this is an empty element */
    if (cols[i] == NULL || cols[i][0] == '\0') {
      continue;
    }

    /* calculate the offset to a character that indicates that the string
     * would need to be escaped, or the \0-terminator if the string would
     * not need to be escaped */
    off = strcspn(cols[i], "\r\n\",");
    if (cols[i][off] == '\0') {
      /* element should not be escaped. 'off' is the string length */
      ret = buf_adata(dst, cols[i], off);
      if (ret < 0) {
        return -1;
      }
    } else {
      /* element needs to be escaped */
      ret = csv_encode_escaped(dst, cols[i]);
      if (ret < 0) {
        return -1;
      }
    }
  }

  /* append trailing CRLF, if non-empty */
  if (ncols > 0) {
    ret = buf_adata(dst, "\r\n", 2);
    if (ret < 0) {
      return -1;
    }
  }

  return 0;
}
