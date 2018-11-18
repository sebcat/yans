#include <string.h>

#include <lib/net/dsts.h>

#define DSTS_FINITED (1 << 0)

static int dst_init(struct dst_ctx *dst, const char *addrs,
    const char *ports, void *dstdata) {
  int ret;

  ret = ip_blocks_init(&dst->addrs, addrs, NULL);
  if (ret < 0) {
    return -1;
  }

  ret = port_ranges_from_str(&dst->ports, ports, NULL);
  if (ret < 0) {
    return -1;
  }

  dst->data = dstdata;
  return 0;
}

static void dst_free(struct dst_ctx *dst) {
  ip_blocks_cleanup(&dst->addrs);
  port_ranges_cleanup(&dst->ports);
}

int dsts_init(struct dsts_ctx *dsts) {
  memset(dsts, 0, sizeof(*dsts));
  if (!buf_init(&dsts->buf, sizeof(struct dst_ctx) * 16)) {
    return -1;
  }

  return 0;
}

void dsts_cleanup(struct dsts_ctx *dsts) {
  struct dst_ctx *curr;
  size_t off;

  for (off = 0;
      off + sizeof(struct dst_ctx) - 1 < dsts->buf.len ;
      off += sizeof(struct dst_ctx)) {
    curr = (struct dst_ctx *)(dsts->buf.data + off);
    dst_free(curr);
  }

  buf_cleanup(&dsts->buf);
}

int dsts_add(struct dsts_ctx *dsts, const char *addrs,
    const char *ports, void *dstdata) {
  int ret;
  struct dst_ctx dst;

  if (addrs == NULL || ports == NULL) {
    return 0; /* adding an empty value */
  }

  ret = dst_init(&dst, addrs, ports, dstdata);
  if (ret < 0) {
    return -1;
  }

  ret = buf_adata(&dsts->buf, &dst, sizeof(dst));
  if (ret < 0) {
    return -1;
  }

  return 0;
}

static int dsts_next_dst(struct dsts_ctx *dsts) {
  while (dsts->dsts_next + sizeof(struct dst_ctx) - 1 < dsts->buf.len) {
    /* load next dst and advance offset */
    memcpy(&dsts->currdst, dsts->buf.data + dsts->dsts_next,
        sizeof(struct dst_ctx));
    dsts->dsts_next += sizeof(struct dst_ctx);
    /* Load next address, if any */
    if (ip_blocks_next(&dsts->currdst.addrs, &dsts->curraddr)) {
      return 1;
    }
  }

  return 0; /* no blocks with addresses present */
}

/* returns zero on completion, non-zero otherwise */
int dsts_next(struct dsts_ctx *dsts, struct sockaddr *dst,
    socklen_t *dstlen, void **data) {
  uint16_t currport;

  if (dsts->buf.len < sizeof(struct dst_ctx)) {
    /* no destinations registered */
    return 0;
  }

  /* (re)initialize ctx for iteration */
  if (!(dsts->flags & DSTS_FINITED)) {
    dsts->dsts_next = 0; /* reset next dst offset */
    if (!dsts_next_dst(dsts)) {
      /* no destinations registered with any addresses in them */
      return 0;
    }
    dsts->flags |= DSTS_FINITED;
  }

again:
  if (!port_ranges_next(&dsts->currdst.ports, &currport)) {
    if (!ip_blocks_next(&dsts->currdst.addrs, &dsts->curraddr)) {
      if (!dsts_next_dst(dsts)) {
        /* we've reached the end */
        dsts->flags &= ~DSTS_FINITED;
        return 0;
      }
    }
    goto again; /* get next port */
  }

  if (dsts->curraddr.u.sa.sa_family == AF_INET) {
    memcpy(dst, &dsts->curraddr.u.sin, sizeof(dsts->curraddr.u.sin));
    ((struct sockaddr_in *)dst)->sin_port = htons(currport);
    *dstlen = sizeof(dsts->curraddr.u.sin);
  } else if (dsts->curraddr.u.sa.sa_family == AF_INET6) {
    memcpy(dst, &dsts->curraddr.u.sin6, sizeof(dsts->curraddr.u.sin6));
    ((struct sockaddr_in6 *)dst)->sin6_port = htons(currport);
    *dstlen = sizeof(dsts->curraddr.u.sin6);
  } else {
    goto again; /* unknown address family - try next one */
  }

  if (data != NULL) {
    *data = dsts->currdst.data;
  }

  return 1;
}
