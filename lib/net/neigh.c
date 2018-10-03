#include <lib/net/neigh.h>

#if defined(__FreeBSD__)
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>


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

static size_t count_entries(char *rt, char *end) {
  size_t count = 0;
  char *curr;
  struct rt_msghdr *rtm;
  struct sockaddr *sa;
  struct sockaddr_dl *sdl;

  for (curr = rt; curr < end; curr += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)curr;
    sa = (struct sockaddr *)(rtm + 1);
    sdl = (struct sockaddr_dl *)((char *)sa + SA_SIZE(sa));

    if (sdl->sdl_family != AF_LINK || !(rtm->rtm_flags & RTF_HOST)) {
      continue;
    }
    count++;
  }

  return count;
}

static size_t copy_entries(struct neigh_entry *entries, size_t offset,
    char *rt, char *end) {
  char *curr;
  struct rt_msghdr *rtm;
  struct sockaddr *sa;
  struct sockaddr_dl *sdl;

  for (curr = rt; curr < end; curr += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)curr;
    sa = (struct sockaddr *)(rtm + 1);
    sdl = (struct sockaddr_dl *)((char *)sa + SA_SIZE(sa));

    if (sdl->sdl_family != AF_LINK || !(rtm->rtm_flags & RTF_HOST)) {
      continue;
    }

    memcpy(&entries[offset].ipaddr.u.sa, sa,
        sa->sa_family == AF_INET6 ? sizeof(struct sockaddr_in6) :
          sizeof(struct sockaddr_in));
    if_indextoname(LLINDEX(sdl), entries[offset].iface);
    if (sdl->sdl_alen == NEIGH_ETHSIZ) {
      /* XXX: assumes ethernet interfaces */
      memcpy(&entries[offset].hwaddr, LLADDR(sdl), NEIGH_ETHSIZ);
    }
    offset++;
  }

  return offset;
}

struct neigh_entry *neigh_get_entries(size_t *nentries, int *err) {
  char *rt4;
  char *end4;
  char *rt6;
  char *end6;
  struct neigh_entry *entries;
  size_t count;
  size_t i;
  int mib[6] = {
      CTL_NET,
      PF_ROUTE,
      0,
      0, /* address family */
      NET_RT_FLAGS,
#ifdef RTF_LLINFO
      RTF_LLINFO,
#else
      0,
#endif
  };


  *nentries = 0;
  mib[3] = AF_INET;
  rt4 = get_mib(mib, 6, &i);
  if (rt4 == NULL) {
    goto fail;
  }
  end4 = rt4 + i;

  mib[3] = AF_INET6;
  rt6 = get_mib(mib, 6, &i);
  if (rt6 == NULL) {
    goto cleanup_rt4;
  }
  end6 = rt6 + i;

  count = count_entries(rt4, end4) + count_entries(rt6, end6);
  entries = calloc(count, sizeof(*entries));
  if (entries == NULL) {
    goto cleanup_rt6;
  }

  i = copy_entries(entries, 0, rt4, end4);
  copy_entries(entries, i, rt6, end6);
  free(rt6);
  free(rt4);
  *nentries = count;
  return entries;

cleanup_rt6:
  free(rt6);
cleanup_rt4:
  free(rt4);
fail:
  if (err) {
    *err = errno;
  }
  return NULL;
}

void neigh_free_entries(struct neigh_entry *entries) {
  if (entries) {
    free(entries);
  }
}

const char *neigh_strerror(int err) {
  if (err == 0) {
    return "unknown neigh error";
  } else {
    return strerror(err);
  }
}

#else /* !defined(__FreeBSD__) */

struct neigh_entry *neigh_get_entries(size_t *nentries, int *err) {
  return NULL;
}

void neigh_free_entries(struct neigh_entry *entries) {

}

const char *neigh_strerror(int err) {
  return "neigh: NYI";
}

#endif
