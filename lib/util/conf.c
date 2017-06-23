#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <lib/util/conf.h>

/*
  char *data;
  size_t datalen;
  char errbuf[CONF_ERRBUFSZ];
*/

/* conf flags */
#define C_FPARSED (1 << 0) /* set when the config is parsed */

#define C_ISSPACE(ch__)   \
  ((ch__) == ' '  ||      \
   (ch__) == '\t')

#define C_ISLINESEP(ch__) \
  ((ch__) == '\r' ||      \
   (ch__) == '\n')

#define C_ISSEP(ch__) ((ch__) == ':')

#define C_ISCOMMENT(ch__) ((ch__) == '#')

#define C_ISESCAPE(ch__) ((ch__) == '\\')

#define SETERR(cfg, ...) \
  snprintf((cfg)->errbuf, CONF_ERRBUFSZ, __VA_ARGS__)

int conf_init(struct conf *cfg, const char *path) {
  int fd = -1;
  void *data = MAP_FAILED;
  size_t len = 0;
  struct stat st;
  char last;

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    SETERR(cfg, "open: %s", strerror(errno));
    goto fail;
  }

  if (fstat(fd, &st) < 0) {
    SETERR(cfg, "fstat: %s", strerror(errno));
    goto fail;
  }

  len = st.st_size;
  if (len > 0) {
    data = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
      SETERR(cfg, "mmap: %s", strerror(errno));
      goto fail;
    }
    /* for symmetry in the parser, we require the conf file to end with a
     * whitespace char (e.g., newline) which can be replaced by a \0-byte. This
     * is likely to cause some problems when editing conf files, but commit
     * hooks and VimScript exists for a reason and can automate that
     * check prior to loading the conf file */
    last = *((char*)data + len - 1);
    if (!C_ISSPACE(last) && !C_ISLINESEP(last)) {
      SETERR(cfg, "conf file does not end with a newline (or any whitespace)");
      goto fail;
    }
  }


  cfg->flags = 0;
  cfg->data = data;
  cfg->datalen = len;
  cfg->errbuf[0] = '\0';
  return 0;

fail:
  if (data != MAP_FAILED) {
    munmap(data, len);
  }

  if (fd >= 0) {
    close(fd);
  }
  return -1;
}

int conf_init_from_str(struct conf *cfg, const char *str) {
  void *data = MAP_FAILED;
  size_t len = 0;
  char last;

  len = strlen(str);
  if (len > 0) {
    last = str[len - 1];
    if (!C_ISSPACE(last) && !C_ISLINESEP(last)) {
      SETERR(cfg, "conf file does not end with a newline (or any whitespace)");
      goto fail;
    }

    data = mmap(NULL, len + 1, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE,
        -1, 0);
    if (data == MAP_FAILED) {
      SETERR(cfg, "mmap: %s", strerror(errno));
      goto fail;
    }

    memcpy(data, str, len+1);
  }

  cfg->flags = 0;
  cfg->data = data;
  cfg->datalen = len;
  cfg->errbuf[0] = '\0';
  return 0;

fail:
  if (data != MAP_FAILED) {
    munmap(data, len);
  }
  return -1;
}

void conf_cleanup(struct conf *cfg) {
  if (cfg->data != MAP_FAILED) {
    munmap(cfg->data, cfg->datalen);
  }
  cfg->data = MAP_FAILED;
  cfg->datalen = 0;
}

static int commit_kv(struct conf *cfg, void *data, struct conf_map *map,
    const char *key, const char *value) {
  struct conf_map *curr;
  unsigned long l = 0;
  char *endptr;

  /* ignore empty values */
  if (*value == '\0') {
    return 0;
  }

  for(curr = map; curr->key != NULL; curr++) {
    if (strcmp(key, curr->key) == 0) {
      if (curr->cfgtype == CONF_TSTR) {
        *(const char**)((char*)data + curr->offset) = value;
      } else if (curr->cfgtype == CONF_TULONG) {
        errno = 0;
        l = strtoul(value, &endptr, 10);
        if (errno != 0 || *endptr != '\0') {
          SETERR(cfg, "unable to parse \"%s\" as an unsigned long", key);
          goto fail;
        }
        *(unsigned long *)((char*)data + curr->offset) = l;
      } else {
        SETERR(cfg, "invalid cfgtype: %d", curr->cfgtype);
        goto fail;
      }
    }
  }

  return 0;

fail:
  return -1;
}

int conf_parse(struct conf *cfg, struct conf_map *map, void *out) {
  size_t pos;
  char ch;
  char *key = NULL;
  char *value = NULL;

  enum {
    FIND_KEY = 0,
    SKIP_LINE,
    SCAN_KEY,
    FIND_VAL,
    SCAN_VAL,
  } S = FIND_KEY;

  if (cfg->flags & C_FPARSED) {
    SETERR(cfg, "conf_parse called more than once");
    goto fail;
  }

  for (pos = 0; pos < cfg->datalen; pos++) {
    ch = cfg->data[pos];
    switch (S) {
    case SKIP_LINE:
      if (C_ISLINESEP(ch)) {
        S = FIND_KEY;
      }
      break;
    case FIND_KEY:
      if (C_ISCOMMENT(ch)) {
        S = SKIP_LINE;
        break;
      }
      if (!C_ISSPACE(ch) && !C_ISLINESEP(ch)) {
        key = cfg->data + pos;
        S = SCAN_KEY;
      }
      break;
    case SCAN_KEY:
      if (C_ISSPACE(ch)) {
        cfg->data[pos] = '\0';
      } else if (C_ISLINESEP(ch) || C_ISCOMMENT(ch)) {
        SETERR(cfg, "unexpected token in key context (\"%c\", offset:%zu)",
            ch, pos);
        goto fail;
      } else if (C_ISSEP(ch)) {
        cfg->data[pos] = '\0';
        S = FIND_VAL;
      }
      break;
    case FIND_VAL:
      if (C_ISLINESEP(ch)) {
        S = FIND_KEY;
        break;
      } else if (C_ISSPACE(ch)) {
        break;
      } else if (C_ISESCAPE(ch)) {
        pos++;
        break;
      }
      value = cfg->data + pos;
      S = SCAN_VAL;
      /* fall-through */
    case SCAN_VAL:
      if (C_ISLINESEP(ch)) {
        cfg->data[pos] = '\0';
        if (commit_kv(cfg, out, map, key, value) < 0) {
          goto fail;
        }
        S = FIND_KEY;
      } else if (C_ISESCAPE(ch)) {
        size_t rest = cfg->datalen - pos;
        if (rest > 1) {
          /* shift all the bytes one step to the left, and pad the end with
           * a newline character */
          char *curr = cfg->data + pos;
          memmove(curr, curr + 1, rest - 1);
          curr[rest-1] = '\n';
        } else {
          SETERR(cfg, "escape character at end of input");
          goto fail;
        }
      }
      break;
    default:
      SETERR(cfg, "unexpected parser state: %d", S);
      goto fail;
    }
  }

  if (S != FIND_KEY) {
    SETERR(cfg, "unexpected end of configuration (S: %d)", S);
    goto fail;
  }

  cfg->flags |= C_FPARSED;
  return 0;
fail:
  cfg->flags |= C_FPARSED;
  return -1;
}
