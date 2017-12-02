#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lib/util/netstring.h>

#include "template.h"

#define NITERS 1000000

static long s2l(const char *s) {
  long res = 0;
  char ch;

  while ((ch = *s) >= '0' && ch <= '7') {
    res = res << 3;
    res += ch - '0';
    s++;
  }

  return res;
}

static char *l2s(char *s, size_t len, long l) {
  while (len > 0) {
    len--;
    s[len] = '0' + (l & 0x07);
    l = l >> 3;
    if (l == 0) {
      break;
    }
  }

  return s + len;
}

static int load_datas(buf_t *b, char *curr, size_t len, size_t *outoffset,
    size_t *nelems) {
  int ret;
  char *elem;
  size_t elemlen;
  size_t offset;
  size_t count;
  struct ycl_data tmp;

  ret = buf_align(b);
  if (ret < 0) {
    return YCL_ERR;
  }

  offset = b->len;
  count = 0;
  while (len > 0) {
    ret = netstring_next(&elem, &elemlen, &curr, &len);
    if (ret != NETSTRING_OK) {
      return YCL_ERR;
    }
    tmp.data = elem;
    tmp.len = elemlen;
    ret = buf_adata(b, &tmp, sizeof(tmp));
    if (ret < 0) {
      return YCL_ERR;
    }
    count++;
  }

  *outoffset = offset;
  *nelems = count;
  return YCL_OK;
}

static int load_strs(buf_t *b, char *curr, size_t len, size_t *outoffset,
    size_t *nelems) {
  int ret;
  char *elem;
  size_t elemlen;
  size_t offset;
  size_t count;

  ret = buf_align(b);
  if (ret < 0) {
    return YCL_ERR;
  }

  offset = b->len;
  count = 0;
  while (len > 0) {
    ret = netstring_next(&elem, &elemlen, &curr, &len);
    if (ret != NETSTRING_OK) {
      return YCL_ERR;
    }
    ret = buf_adata(b, &elem, sizeof(elem));
    if (ret < 0) {
      return YCL_ERR;
    }
    count++;
  }

  *outoffset = offset;
  *nelems = count;
  return YCL_OK;
}

static int load_longs(buf_t *b, char *curr, size_t len,
    size_t *outoffset, size_t *nelems) {
  int ret;
  char *elem;
  size_t elemlen;
  size_t offset;
  size_t count;
  long val;

  ret = buf_align(b);
  if (ret < 0) {
    return YCL_ERR;
  }

  offset = b->len;
  count = 0;
  while (len > 0) {
    ret = netstring_next(&elem, &elemlen, &curr, &len);
    if (ret != NETSTRING_OK) {
      return YCL_ERR;
    }
    val = s2l(elem);
    ret = buf_adata(b, &val, sizeof(val));
    if (ret < 0) {
      return YCL_ERR;
    }
    count++;
  }

  *outoffset = offset;
  *nelems = count;
  return YCL_OK;
}


int ycl_msg_create_baz(struct ycl_msg *msg, struct ycl_msg_baz *r) {
  int ret;
  int status = YCL_ERR;

  if (ycl_msg_use_optbuf(msg) < 0) {
    goto done;
  }

  ycl_msg_reset(msg);
  if (r->s != NULL) {
    ret = netstring_append_pair(&msg->mbuf, "s", 1, r->s, strlen(r->s));
    if (ret != NETSTRING_OK) {
      goto done;
    }
  }

  if (r->d.len > 0) {
    ret = netstring_append_pair(&msg->mbuf, "d", 1, r->d.data, r->d.len);
    if (ret != NETSTRING_OK) {
      goto done;
    }
  }

  if (r->l != 0) {
    char numbuf[48];
    char *num;

    num = l2s(numbuf, sizeof(numbuf), r->l);
    ret = netstring_append_pair(&msg->mbuf, "l", 1, num,
        numbuf + sizeof(numbuf) - num);
    if (ret != NETSTRING_OK) {
      goto done;
    }
  }

  if (r->darr != NULL && r->ndarr > 0) {
    struct ycl_data *ptr;
    size_t i;
    size_t nelems;

    ptr = r->darr;
    nelems = r->ndarr;
    buf_clear(&msg->optbuf);
    for (i = 0; i < nelems; i++) {
      ret = netstring_append_buf(&msg->optbuf, ptr[i].data, ptr[i].len);
      if (ret != NETSTRING_OK) {
        goto done;
      }
    }
    ret = netstring_append_pair(&msg->mbuf, "darr", 4, msg->optbuf.data,
        msg->optbuf.len);
    if (ret != NETSTRING_OK) {
      goto done;
    }
  }

  if (r->sarr != NULL && r->nsarr > 0) {
    const char **ptr;
    size_t i;
    size_t nelems;

    ptr = r->sarr;
    nelems = r->nsarr;
    buf_clear(&msg->optbuf);
    for (i = 0; i < nelems; i++) {
      ret = netstring_append_buf(&msg->optbuf, ptr[i], strlen(ptr[i]));
      if (ret != NETSTRING_OK) {
        goto done;
      }
    }
    ret = netstring_append_pair(&msg->mbuf, "sarr", 4, msg->optbuf.data,
        msg->optbuf.len);
    if (ret != NETSTRING_OK) {
      goto done;
    }
  }

  if (r->larr != NULL && r->nlarr > 0) {
    long *ptr;
    size_t i;
    size_t nelems;
    char numbuf[48];
    char *num;

    ptr = r->larr;
    nelems = r->nlarr;
    buf_clear(&msg->optbuf);
    for (i = 0; i < nelems; i++) {
      num = l2s(numbuf, sizeof(numbuf), ptr[i]);
      ret = netstring_append_buf(&msg->optbuf, num,
        numbuf + sizeof(numbuf) - num);
      if (ret != NETSTRING_OK) {
        goto done;
      }
    }

    ret = netstring_append_pair(&msg->mbuf, "larr", 4, msg->optbuf.data,
        msg->optbuf.len);
    if (ret != NETSTRING_OK) {
      goto done;
    }
  }

  ret = netstring_append_buf(&msg->buf, msg->mbuf.data, msg->mbuf.len);
  if (ret != NETSTRING_OK) {
    goto done;
  }

  status = YCL_OK;
done:
  return status;
}

int ycl_msg_parse_baz(struct ycl_msg *msg, struct ycl_msg_baz *r) {
  struct netstring_pair pair;
  int result = YCL_ERR;
  char *curr;
  size_t len;
  int ret;
  size_t pairoff;
  size_t currpair;
  static const char *names[] = {
    "s",
    "d",
    "l",
    "darr",
    "sarr",
    "larr",
  };

  memset(r, 0, sizeof(*r));
  buf_clear(&msg->mbuf);
  ret = netstring_parse(&curr, &len, msg->buf.data, msg->buf.len);
  if (ret != NETSTRING_OK) {
    goto done;
  }

  /* ycl_msg_create_* will create messages in a determinable order. We use
   * that order for our best case, and do a linear search for our worst
   * case. */
  for (pairoff = 0;
      netstring_next_pair(&pair, &curr, &len) == NETSTRING_OK;
      pairoff = (pairoff + 1) % (sizeof(names) / sizeof(char*))) {
    currpair = pairoff;
    do {
      if (strcmp(pair.key, names[currpair]) == 0) {
        switch(currpair) {
          case 0: /* d */
			r->d.data = pair.value;
			r->d.len = pair.valuelen;
            break;
          case 1: /* s */
			r->s = pair.value;
            break;
          case 2: /* l */
            r->l = s2l(pair.value);
            break;
          case 3: /* darr */
            ret = load_datas(&msg->mbuf, pair.value, pair.valuelen,
                (size_t*)&r->darr, &r->ndarr);
            if (ret != YCL_OK) {
              goto done;
            }
            break;
          case 4: /* sarr */
            ret = load_strs(&msg->mbuf, pair.value, pair.valuelen,
                (size_t*)&r->sarr, &r->nsarr);
            if (ret != YCL_OK) {
              goto done;
            }
            break;
          case 5: /* larr */
            ret = load_longs(&msg->mbuf, pair.value, pair.valuelen,
                (size_t*)&r->larr, &r->nlarr);
            if (ret != YCL_OK) {
              goto done;
            }
            break;
          default:
            /* If this code is reached, it means currpair is outside of the
             * names array, which is an inconsistent state. */
            abort();
        }
        break;
      }
      currpair = (currpair + 1) % (sizeof(names) / sizeof(char*));
    } while (currpair != pairoff);
  }

  if (r->ndarr > 0) {
    r->darr = (void*)(msg->mbuf.data + (size_t)r->darr);
  }

  if (r->nsarr > 0) {
    r->sarr = (void*)(msg->mbuf.data + (size_t)r->sarr);
  }

  if (r->nlarr > 0) {
    r->larr = (void*)(msg->mbuf.data + (size_t)r->larr);
  }

  result = YCL_OK;
done:
  return result;
}

static void print_baz(struct ycl_msg_baz *baz) {
  int i;

  printf("d: \"%*s\"\ns: \"%s\"\nl: \"%lx\"\n",
      (int)baz->d.len, baz->d.data, baz->s, baz->l);

  if (baz->ndarr > 0) {
    for (i = 0; i < baz->ndarr; i++) {
      printf("darr[%d]: \"%*s\"\n", i, (int)baz->darr[i].len,
          baz->darr[i].data);
    }
  }

  if (baz->nsarr > 0) {
    for (i = 0; i < baz->nsarr; i++) {
      printf("sarr[%d]: \"%s\"\n", i, baz->sarr[i]);
    }
  }

  if (baz->nlarr > 0) {
    for (i = 0; i < baz->nlarr; i++) {
      printf("larr[%d]: \"%lx\"\n", i, baz->larr[i]);
    }
  }
}

int main() {
  struct ycl_msg msg;
  int ret;
  int i;
  int status = EXIT_FAILURE;
  static const char *sarrval[] = {"foo", "bar", "baz",
      "somelongerstringfortestingstringsthatareabitlonger"};
  static struct ycl_data darrval[] = {
    {.data = "darr0", .len = 5},
    {.data = "darr1", .len = 5},
  };
  static long larrval[] = {666L, 667L};
  struct ycl_msg_baz res;
  static struct ycl_msg_baz data = {
    .d = {.data = "fooTHISSHOULDNOTBESEEN", .len = 3},
    .s = "bar",
    .l = 0x7fffffffffffffffL,
    .darr = darrval,
    .ndarr = sizeof(darrval) / sizeof(struct ycl_data),
    .sarr = sarrval,
    .nsarr = sizeof(sarrval) / sizeof(const char *),
    .larr = larrval,
    .nlarr = sizeof(larrval) / sizeof(long),
  };

  ycl_msg_init(&msg);

  for (i = 0; i < NITERS; i++) {
    memset(&res, 0, sizeof(res));
    ret = ycl_msg_create_baz(&msg, &data);
    if (ret != YCL_OK) {
      fprintf(stderr, "ycl_msg_create_baz failure\n");
      goto done;
    }

    ret = ycl_msg_parse_baz(&msg, &res);
    if (ret != YCL_OK) {
      fprintf(stderr, "ycl_msg_parse_baz failure\n");
      goto done;
    }
  }

  print_baz(&res);

  status = EXIT_SUCCESS;
done:
  ycl_msg_cleanup(&msg);
  return status;
}
