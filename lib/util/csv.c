#include <string.h>
#include <limits.h>
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

int csv_reader_init(struct csv_reader *r) {
  if (buf_init(&r->data, 4096) == NULL) {
    return -1;
  }

  if (buf_init(&r->cols, 512) == NULL) {
    buf_cleanup(&r->data);
    return -1;
  }

  return 0;
}

void csv_reader_cleanup(struct csv_reader *r) {
  buf_cleanup(&r->data);
  buf_cleanup(&r->cols);
}

static int read_escaped(struct csv_reader *r, FILE *in) {
  int cont = 1;
  int ch;
  size_t off;

  off = r->data.len; /* save initial offset */
  while (1) {
    ch = fgetc(in);
    switch (ch) {
    case EOF:
      cont = 0;
      goto done;
    case '"':
      ch = fgetc(in);
      switch (ch) {
      case ',':
        goto done;
      case '\r':
        ch = fgetc(in);
        if (ch != '\n' && ch != EOF) {
          ungetc(ch, in);
        }
        /* fallthrough */
      case '\n':
      case EOF:
        cont = 0;
        goto done;
      default:
        break; /* will fallthrough on the outer switch to its default */
      }
      /* fallthrough */
    default:
      buf_achar(&r->data, ch);
      break;
    }
  }

done:
  buf_achar(&r->data, '\0');
  buf_adata(&r->cols, &off, sizeof(off));
  return cont;
}

static int read_unescaped(struct csv_reader *r, FILE *in) {
  int ch;
  int cont = 1;
  size_t off;

  off = r->data.len; /* save initial offset */
  while (1) {
    ch = fgetc(in);
    switch (ch) {
      case EOF:
      case '\r':
      case '\n':
        cont = 0;
        goto done;
      case ',':
        goto done;
      default:
        buf_achar(&r->data, ch);
        break;
    }
  }

done:
  buf_achar(&r->data, '\0');
  buf_adata(&r->cols, &off, sizeof(off));
  return cont;
}

static int read_empty(struct csv_reader *r) {
  size_t off;

  off = r->data.len;
  buf_achar(&r->data, '\0');
  buf_adata(&r->cols, &off, sizeof(off));
  return 0;
}

int csv_read_row(struct csv_reader *r, FILE *in) {
  int ch;
  int cont = 1;
  int lastsep = 0; /* true if last column ended with a separator (,) */

  /* clear any previous state */
  buf_clear(&r->data);
  buf_clear(&r->cols);

  while (cont) {
    ch = fgetc(in);
    switch (ch) {
    case '\r':
      ch = fgetc(in);
      if (ch != '\n' && ch != EOF) {
        ungetc(ch, in);
      }
      /* fallthrough */
    case '\n':
    case EOF:
      if (lastsep) {
        /* last read column ended with a separator, and now we have an
         * EOL/EOF, meaning that the last column in the line was empty */
        read_empty(r);
      }
      cont = 0;
      break;
    case '"':
      lastsep = cont = read_escaped(r, in);
      break;
    case ',':
      read_empty(r);
      lastsep = 1;
      break;
    default:
      ungetc(ch, in);
      lastsep = cont = read_unescaped(r, in);
      break;
    }
  }

  return ferror(in) ? -1 : 0;
}
