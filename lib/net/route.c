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

static void copy_table_entry(struct route_table_entry *ent,
    struct rt_msghdr *rt_msg, int af) {
  struct sockaddr *sa;
  size_t len;
  int flags;

  memset(ent, 0, sizeof(struct route_table_entry));
  ent->gw_ifindex = -1; /* filled in if the gw entry is a link address */
  ent->ifindex = (int)rt_msg->rtm_index;

  /* first sockaddr is directly after the rt_msghdr struct */
  sa = (struct sockaddr *)(rt_msg + 1);

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

  /* copy netmask, if any */
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

static int setup_table_entries(struct route_table *rt, int af, char *data,
    size_t len) {
  struct route_table_entry *ents = NULL;
  struct rt_msghdr *rt_msg;
  size_t nelems = 0;
  size_t off;
  size_t i;
  int status = 0;

  /* count the number of entries */
  for (off = 0; off < len; off += rt_msg->rtm_msglen) {
    rt_msg = (struct rt_msghdr*)(data + off);
    if (rt_msg->rtm_version == RTM_VERSION) {
      nelems++;
    }
  }

  if (nelems == 0) {
    goto done;
  }

  /* allocate memory for the entries, if possible */
  ents = malloc(nelems * sizeof(struct route_table_entry));
  if (ents == NULL) {
    SETERR(rt, "malloc", errno);
    status = -1;
    goto done;
  }

  for (i = off = 0; off < len; off += rt_msg->rtm_msglen) {
    rt_msg = (struct rt_msghdr*)(data + off);
    if (rt_msg->rtm_version == RTM_VERSION) {
      copy_table_entry(&ents[i], rt_msg, af);
      i++;
    }
  }

  if (af == AF_INET) {
    rt->entries_ip4 = ents;
    rt->nentries_ip4 = nelems;
    ents = NULL;
  } else if (af == AF_INET6) {
    rt->entries_ip6 = ents;
    rt->nentries_ip6 = nelems;
    ents = NULL;
  } else {
    goto done;
  }

done:
  if (ents != NULL) {
    free(ents);
  }
  return status;
}


static int _route_table_init(struct route_table *rt, int af, int fibnum) {
  int mib[7];
  int ret;
  size_t needed = 0;
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
  /* XXX: TOCTOU between the size check and the actual copying of the table
   * from sysctl */
  ret = sysctl(mib, sizeof(mib)/sizeof(int), NULL, &needed, NULL, 0);
  if (ret < 0) {
    SETERR(rt, "sysctl estimate", errno);
    goto out;
  }

  if (needed == 0) {
    return 0;
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

  /* allocate and copy the routing table entries (if any) to */
  ret = setup_table_entries(rt, af, buf, needed);
  if (ret < 0) {
    goto out;
  }

  /* success */
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

void route_table_cleanup(struct route_table *rt) {
  if (rt->entries_ip4 != NULL) {
    free(rt->entries_ip4);
    rt->entries_ip4 = NULL;
    rt->nentries_ip4 = 0;
  }

  if (rt->entries_ip6 != NULL) {
    free(rt->entries_ip6);
    rt->entries_ip6 = NULL;
    rt->nentries_ip6 = 0;
  }
}

int route_table_init(struct route_table *rt) {
  int ret;
  int fibnum;

  fibnum = get_current_fibnum(rt);
  if (fibnum < 0) {
    return -1;
  }

  memset(rt, 0, sizeof(*rt));
  ret = _route_table_init(rt, AF_INET, fibnum);
  if (ret < 0) {
    return -1;
  }

  ret = _route_table_init(rt, AF_INET6, fibnum);
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

#endif
