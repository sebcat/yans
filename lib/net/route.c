#if defined(__FreeBSD__)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/route.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <lib/net/route.h>

#define SETERR(rt__, errprefix__, errno__) \
  (rt__)->errprefix = (errprefix__);       \
  (rt__)->errnum = (errno__);


static int memcmprev(const void *a, const void *b, size_t n) {
  const unsigned char *left = a;
  const unsigned char *right = b;
  size_t i;

  for (i = 0; i < n; i++) {
    if (left[n-1-i] != right[n-1-i]) {
      return (left[n-1-i] - right[n-1-i]);
    }
  }

  return 0;
}

/* TODO: Once we have a suitable Linux implementation, move the comparator
 *       to the platform-independent section */
static int cmprtent(const void *a, const void *b) {
  const struct route_table_entry *left = a;
  const struct route_table_entry *right = b;

  /* address family order */
  if (left->addr.sa.sa_family != right->addr.sa.sa_family) {
    return left->addr.sa.sa_family - right->addr.sa.sa_family;
  }

  /* lookup order (most-specific to least specific) */
  if (left->addr.sa.sa_family == AF_INET) {
    return memcmprev(&right->mask.sin.sin_addr.s_addr,
        &left->mask.sin.sin_addr.s_addr, sizeof(in_addr_t));
  } else if (left->addr.sa.sa_family == AF_INET6) {
    return memcmprev(&right->mask.sin6.sin6_addr,
        &left->mask.sin6.sin6_addr, sizeof(struct in6_addr));
  }

  return 0; /* on fallthrough for w/e reason */
}

void route_table_strerror(struct route_table *rt, char *buf, size_t len) {
  if (len > 0) {
    if (rt->errnum != 0) {
      snprintf(buf, len, "%s: %s", rt->errprefix, strerror(rt->errnum));
    } else {
      *buf = '\0';
    }
  }
}

static char *get_mib(int *mib, u_int nentries, size_t *sz) {
  size_t needed;
  int ret;
  char *buf = NULL;
  char *tmp = NULL;

  ret = sysctl(mib, nentries, NULL, &needed, NULL, 0);
  if (ret < 0 || needed == 0) {
    goto fail;
  }

  while(1) {
    tmp = realloc(buf, needed);
    if (tmp == NULL) {
      goto fail;
    }
    buf = tmp;
    ret = sysctl(mib, nentries, buf, &needed, NULL, 0);
    if (ret == 0 || errno != ENOMEM) {
      break;
    }
    needed += needed / 8;
  }

  if (ret < 0) {
    goto fail;
  }

  *sz = needed;
  return buf;

fail:
  if (buf != NULL) {
    free(buf);
  }

  return NULL;
}

static void copy_table_entry(struct route_table_entry *ent,
    struct rt_msghdr *rt_msg) {
  struct sockaddr *sa;
  size_t len;
  int flags;
  int af;

  memset(ent, 0, sizeof(struct route_table_entry));
  ent->gw_ifindex = -1; /* filled in if the gw entry is a link address */
  ent->ifindex = (int)rt_msg->rtm_index;

  /* first sockaddr is directly after the rt_msghdr struct */
  sa = (struct sockaddr *)(rt_msg + 1);
  af = sa->sa_family;

  /* copy route dst address, if any */
  if (rt_msg->rtm_addrs & RTA_DST) {
    len = (sa->sa_len + sizeof(long)-1) & ~(sizeof(long)-1);
    memcpy(&ent->addr, sa,
        (len > sizeof(ent->addr) ? sizeof(ent->addr) : len));
    sa = (struct sockaddr *)((char*)sa + len);
  }

  /* copy gateway address, if any */
  if (rt_msg->rtm_addrs & RTA_GATEWAY) {
    len = (sa->sa_len + sizeof(long)-1) & ~(sizeof(long)-1);
    memcpy(&ent->gw, sa, (len > sizeof(ent->gw) ? sizeof(ent->gw) : len));
    if (sa->sa_family == AF_LINK) {
      ent->gw_ifindex = (int)((struct sockaddr_dl*)sa)->sdl_index;
    }
    sa = (struct sockaddr *)((char*)sa + len);
  }

  /* copy netmask, or set it to /32 or /128 if no netmask */
  if (rt_msg->rtm_addrs & RTA_NETMASK) {
    len = (sa->sa_len + sizeof(long)-1) & ~(sizeof(long)-1);
    memcpy(&ent->mask, sa,
        (len > sizeof(ent->mask) ? sizeof(ent->mask) : len));

    /* on FreeBSD 10.3-RELEASE-p18 and possibly others, it appears as if only
     * the netmask address field is filled in, and most other fields in the
     * netmask is left with garbage. This means it's problematic to call it
     * with e.g., getnameinfo or other socket APIs. We clean it up here. */
    if (af == AF_INET6) {
      ent->mask.sa.sa_family = AF_INET6;
      ent->mask.sin6.sin6_scope_id = 0;
      ent->mask.sin6.sin6_flowinfo = 0;
    } else if (af == AF_INET) {
      ent->mask.sa.sa_family = AF_INET;
    }
    sa = (struct sockaddr *)((char*)sa + len);
  } else if (af == AF_INET) {
    /* IPv4 entry, no netmask */
    ent->mask.sa.sa_family = AF_INET;
    ent->mask.sin.sin_addr.s_addr = 0xffffffff;
  } else if (af == AF_INET6) {
    /* IPv6 entry, no netmask */
      ent->mask.sa.sa_family = AF_INET6;
      ent->mask.sin6.sin6_scope_id = 0;
      ent->mask.sin6.sin6_flowinfo = 0;
      memcpy(&ent->mask.sin6.sin6_addr, "\xff\xff\xff\xff\xff\xff\xff\xff"
          "\xff\xff\xff\xff\xff\xff\xff\xff", 16);
  }

  flags = rt_msg->rtm_flags;

  if (flags & RTF_UP) {
    ent->flags |= RTENTRY_UP;
  }

  if (flags & RTF_GATEWAY) {
    ent->flags |= RTENTRY_GW;
  }

  if (flags & RTF_HOST) {
    ent->flags |= RTENTRY_HOST;
  }

  if (flags & RTF_REJECT) {
    ent->flags |= RTENTRY_REJECT;
  }

  if (flags & RTF_STATIC) {
    ent->flags |= RTENTRY_STATIC;
  }
}

static size_t count_entries(const char *data, size_t len) {
  struct rt_msghdr *rt_msg;
  size_t nelems = 0;
  size_t off;

  /* count the number of entries */
  for (off = 0; off < len; off += rt_msg->rtm_msglen) {
    rt_msg = (struct rt_msghdr*)(data + off);
    if (rt_msg->rtm_version == RTM_VERSION) {
      nelems++;
    }
  }

  return nelems;
}

static size_t copy_entries(struct route_table_entry *ents, size_t entoff,
    char *data, size_t len) {
  struct rt_msghdr *rt_msg;
  size_t off = 0;

  for (off = 0; off < len; off += rt_msg->rtm_msglen) {
    rt_msg = (struct rt_msghdr*)(data + off);
    if (rt_msg->rtm_version == RTM_VERSION) {
      copy_table_entry(&ents[entoff], rt_msg);
      entoff++;
    }
  }

  return entoff;
}

static int _route_table_init(struct route_table *rt, int fibnum) {
  int status = -1;
  char *ents4;
  size_t ents4_sz;
  char *ents6;
  size_t ents6_sz;
  size_t cpoff;
  struct route_table_entry *ents = NULL;
  size_t nents;
  int mib[7] = {
      CTL_NET,
      PF_ROUTE,
      0,
      0, /* address family */
      NET_RT_DUMP,
      0,
      fibnum,
  };

  /* zero-init rt */
  memset(rt, 0, sizeof(*rt));

  /* get the IPv4 entries */
  mib[3] = AF_INET;
  ents4 = get_mib(mib, 7, &ents4_sz);
  if (ents4 == NULL) {
    SETERR(rt, "get_mib AF_INET", errno);
    goto fail;
  }

  /* get the IPv6 entries */
  mib[3] = AF_INET6;
  ents6 = get_mib(mib, 7, &ents6_sz);
  if (ents6 == NULL) {
    SETERR(rt, "get_mib AF_INET6", errno);
    goto cleanup_ents4;
  }

  /* count the number of route_table_entry fields needed */
  nents = count_entries(ents4, ents4_sz);
  nents += count_entries(ents6, ents6_sz);
  if (nents == 0) {
    goto done;
  }

  ents = calloc(nents, sizeof(struct route_table_entry));
  if (ents == NULL) {
    SETERR(rt, "calloc", errno);
    goto cleanup_ents6;
  }

  cpoff = copy_entries(ents, 0, ents4, ents4_sz);
  copy_entries(ents, cpoff, ents6, ents6_sz);

  /* success */
done:
  status = 0;
  rt->nentries = nents;
  rt->entries = ents;

cleanup_ents6:
  free(ents6);
cleanup_ents4:
  free(ents4);
fail:
  return status;
}

static int get_current_fibnum(struct route_table *rt) {
  int fibnum;
  size_t fibnumsz = sizeof(int);
  int ret;

  ret = sysctlbyname("net.my_fibnum", &fibnum, &fibnumsz, NULL, 0);
  if (ret < 0) {
    SETERR(rt, "sysctl net.myfibnum", errno);
    return -1;
  }

  return fibnum;
}

void route_table_cleanup(struct route_table *rt) {
  if (rt->entries != NULL) {
    free(rt->entries);
    rt->entries = NULL;
    rt->nentries = 0;
  }
}

int route_table_init(struct route_table *rt) {
  int ret;
  int fibnum;

  fibnum = get_current_fibnum(rt);
  if (fibnum < 0) {
    return -1;
  }

  ret = _route_table_init(rt, fibnum);
  if (ret < 0) {
    return -1;
  }

  /* sort the routes in protocol and lookup order
   * (most-specific to least-specific) */
  if (rt->nentries > 0) {
    qsort(rt->entries, rt->nentries, sizeof(*rt->entries), cmprtent);
  }

  return 0;
}

#else /* defined(__FreeBSD) */

#include <string.h>
#include <stdio.h>

#include <lib/net/route.h>

int route_table_init(struct route_table *rt) {
  memset(rt, 0, sizeof(*rt));
  return 0;
}

void route_table_cleanup(struct route_table *rt) {

}

void route_table_strerror(struct route_table *rt, char *buf, size_t len) {
  snprintf(buf, len, "route_dummy: NYI");
}

#endif
