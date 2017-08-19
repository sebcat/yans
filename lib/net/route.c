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

void route_table_strerror(struct route_table *rt, char *buf, size_t len) {
  if (len > 0) {
    if (rt->errnum != 0) {
      snprintf(buf, len, "%s: %s", rt->errprefix, strerror(rt->errnum));
    } else {
      *buf = '\0';
    }
  }
}

static int _route_table_init(struct route_table *rt, int af, int fibnum,
    struct route_table_entries *out) {
  int mib[7];
  int ret;
  size_t needed;
  char *buf = NULL;
  int status = -1;

  /* set up the MIB */
  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = af;
  mib[4] = NET_RT_DUMP;
  mib[5] = 0;
  mib[6] = fibnum;

  /* determine the size in bytes of the routing table */
  ret = sysctl(mib, sizeof(mib)/sizeof(int), NULL, &needed, NULL, 0);
  if (ret < 0) {
    SETERR(rt, "sysctl estimate", errno);
    goto out;
  }

  /* allocate memory for the routing table */
  buf = malloc(needed);
  if (buf == NULL) {
    SETERR(rt, "malloc", errno);
    goto out;
  }

  /* copy the routing table to the allocated buffer */
  ret = sysctl(mib, sizeof(mib)/sizeof(int), buf, &needed, NULL, 0);
  if (ret < 0) {
    SETERR(rt, "sysctl", errno);
    goto out;
  }

  out->buf = buf;
  out->len = needed;
  buf = NULL;
  status = 0;

out:
  if (buf != NULL) {
    free(buf);
  }

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

static int route_table_next(struct route_table_entries *ents,
    struct route_table_entry *out) {
  struct rt_msghdr *rt_msg;
  struct sockaddr *sa;
  size_t len;
  int flags;

  do {
    if (ents->off >= ents->len) {
      return 0;
    }

    rt_msg = (struct rt_msghdr*)(ents->buf + ents->off);
    ents->off += rt_msg->rtm_msglen;
  } while (rt_msg->rtm_version != RTM_VERSION);

  memset(out, 0, sizeof(*out));
  out->gw_ifindex = -1; /* filled in if the gw entry is a link address */
  out->ifindex = (int)rt_msg->rtm_index;

  /* first sockaddr is directly after the rt_msghdr struct */
  sa = (struct sockaddr *)(rt_msg + 1);

  /* copy route dst address, if any */
  if (rt_msg->rtm_addrs & RTA_DST) {
    len = (sa->sa_len + sizeof(long)-1) & ~(sizeof(long)-1);
    memcpy(&out->addr, sa,
        (len > sizeof(out->addr) ? sizeof(out->addr) : len));
    sa = (struct sockaddr *)((char*)sa + len);
  }

  /* copy gateway address, if any */
  if (rt_msg->rtm_addrs & RTA_GATEWAY) {
    len = (sa->sa_len + sizeof(long)-1) & ~(sizeof(long)-1);
    memcpy(&out->gw, sa, (len > sizeof(out->gw) ? sizeof(out->gw) : len));
    if (sa->sa_family == AF_LINK) {
      out->gw_ifindex = (int)((struct sockaddr_dl*)sa)->sdl_index;
    }
    sa = (struct sockaddr *)((char*)sa + len);
  }

  /* copy netmask, if any */
  if (rt_msg->rtm_addrs & RTA_NETMASK) {
    len = (sa->sa_len + sizeof(long)-1) & ~(sizeof(long)-1);
    memcpy(&out->mask, sa,
        (len > sizeof(out->mask) ? sizeof(out->mask) : len));
    sa = (struct sockaddr *)((char*)sa + len);
  }

  flags = rt_msg->rtm_flags;

  if (flags & RTF_UP) {
    out->flags |= RTENTRY_UP;
  }

  if (flags & RTF_GATEWAY) {
    out->flags |= RTENTRY_GW;
  }

  if (flags & RTF_HOST) {
    out->flags |= RTENTRY_HOST;
  }

  if (flags & RTF_REJECT) {
    out->flags |= RTENTRY_REJECT;
  }

  if (flags & RTF_STATIC) {
    out->flags |= RTENTRY_STATIC;
  }

  return 1;
}

int route_table_next_ip4(struct route_table *rt,
    struct route_table_entry *out) {
  int ret;
  ret = route_table_next(&rt->ip4, out);
  if (ret == 1) {
    /* on FreeBSD 10.3-RELEASE-p18 and possibly others, it appears as if only
     * the netmask address field is filled in, and most other fields in the
     * netmask is left with garbage. This means it's problematic to call it
     * with e.g., getnameinfo or other socket APIs. We clean it up here. */
    out->mask.sa.sa_family = AF_INET;
  }

  return ret;
}

int route_table_next_ip6(struct route_table *rt,
    struct route_table_entry *out) {
  int ret;
  ret = route_table_next(&rt->ip6, out);
  if (ret == 1) {
     /* see route_table_next_ip4 on why this is done */
     out->mask.sa.sa_family = AF_INET6;
     out->mask.sin6.sin6_scope_id = 0;
     out->mask.sin6.sin6_flowinfo = 0;
  }

  return ret;
}

static void route_table_entries_cleanup(struct route_table_entries *ents) {
  if (ents->buf != NULL) {
    free(ents->buf);
  }

  memset(ents, 0, sizeof(*ents));
}

void route_table_cleanup(struct route_table *rt) {
  route_table_entries_cleanup(&rt->ip4);
  route_table_entries_cleanup(&rt->ip6);
}

int route_table_init(struct route_table *rt) {
  int ret;
  int fibnum;

  fibnum = get_current_fibnum(rt);
  if (fibnum < 0) {
    return -1;
  }

  memset(rt, 0, sizeof(*rt));
  ret = _route_table_init(rt, AF_INET, fibnum, &rt->ip4);
  if (ret < 0) {
    return -1;
  }

  ret = _route_table_init(rt, AF_INET6, fibnum, &rt->ip6);
  if (ret < 0) {
    route_table_cleanup(rt);
    return -1;
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

int route_table_next_ip4(struct route_table *rt,
    struct route_table_entry *out) {
  return 0;
}

int route_table_next_ip6(struct route_table *rt,
    struct route_table_entry *out) {
  return 0;
}

#endif
