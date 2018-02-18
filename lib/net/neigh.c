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


static char *get_rt(int af, char **end) {
  size_t needed;
  int ret;
  char *buf = NULL;
  char *tmp = NULL;
  int mib[6] = {
      CTL_NET,
      PF_ROUTE,
      0,
      af,
      NET_RT_FLAGS,
#ifdef RTF_LLINFO
      RTF_LLINFO,
#else
      0,
#endif
  };

  ret = sysctl(mib, 6, NULL, &needed, NULL, 0);
  if (ret < 0 || needed == 0) {
    goto fail;
  }

  while(1) {
    tmp = realloc(buf, needed);
    if (tmp == NULL) {
      goto fail;
    }
    buf = tmp;
    ret = sysctl(mib, 6, buf, &needed, NULL, 0);
    if (ret == 0 || errno != ENOMEM) {
      break;
    }
    needed += needed / 8;
  }

  if (ret < 0) {
    goto fail;
  }

  *end = buf + needed;
  return buf;

fail:
  if (buf != NULL) {
    free(buf);
  }

  return NULL;
}

struct neigh_ip4_entry *neigh_get_ip4_entries(size_t *nentries, int *err) {
  char *rt;
  char *curr;
  char *end;
  struct rt_msghdr *rtm;
  struct sockaddr_in *sin;
  struct sockaddr_dl *sdl;
  struct neigh_ip4_entry *entries;
  size_t count;
  size_t i;

  *nentries = 0;
  rt = get_rt(AF_INET, &end);
  if (rt == NULL) {
    goto fail;
  }

  count = 0;
  for (curr = rt; curr < end; curr += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)curr;
    sin = (struct sockaddr_in *)(rtm + 1);
    sdl = (struct sockaddr_dl *)((char *)sin + SA_SIZE(sin));

    if (sdl->sdl_family != AF_LINK || !(rtm->rtm_flags & RTF_HOST)) {
      continue;
    }
    count++;
  }

  entries = calloc(count, sizeof(*entries));
  if (entries == NULL) {
    goto cleanup_rt;
  }

  for (i = 0, curr = rt; curr < end; curr += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)curr;
    sin = (struct sockaddr_in *)(rtm + 1);
    sdl = (struct sockaddr_dl *)((char *)sin + SA_SIZE(sin));

    if (sdl->sdl_family != AF_LINK || !(rtm->rtm_flags & RTF_HOST)) {
      continue;
    }

    /* XXX: assumes ethernet interfaces */
    entries[i].sin = *sin;
    if_indextoname(LLINDEX(sdl), entries[i].iface);
    if (sdl->sdl_alen == NEIGH_ETHSIZ) {
      memcpy(&entries[i].hwaddr, LLADDR(sdl), NEIGH_ETHSIZ);
    }
    i++;
  }

  free(rt);
  *nentries = count;
  return entries;

cleanup_rt:
  free(rt);
fail:
  if (err) {
    *err = errno;
  }
  return NULL;
}

struct neigh_ip6_entry *neigh_get_ip6_entries(size_t *nentries, int *err) {
  char *rt;
  char *curr;
  char *end;
  struct rt_msghdr *rtm;
  struct sockaddr_in6 *sin6;
  struct sockaddr_dl *sdl;
  struct neigh_ip6_entry *entries;
  size_t count;
  size_t i;

  *nentries = 0;
  rt = get_rt(AF_INET6, &end);
  if (rt == NULL) {
    goto fail;
  }

  count = 0;
  for (curr = rt; curr < end; curr += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)curr;
    sin6 = (struct sockaddr_in6 *)(rtm + 1);
    sdl = (struct sockaddr_dl *)((char *)sin6 + SA_SIZE(sin6));

    if (sdl->sdl_family != AF_LINK || !(rtm->rtm_flags & RTF_HOST)) {
      continue;
    }
    count++;
  }

  entries = calloc(count, sizeof(*entries));
  if (entries == NULL) {
    goto cleanup_rt;
  }

  for (i = 0, curr = rt; curr < end; curr += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)curr;
    sin6 = (struct sockaddr_in6 *)(rtm + 1);
    sdl = (struct sockaddr_dl *)((char *)sin6 + SA_SIZE(sin6));

    if (sdl->sdl_family != AF_LINK || !(rtm->rtm_flags & RTF_HOST)) {
      continue;
    }

    /* XXX: assumes ethernet interfaces */
    entries[i].sin6 = *sin6;
    if_indextoname(LLINDEX(sdl), entries[i].iface);
    if (sdl->sdl_alen == NEIGH_ETHSIZ) {
      memcpy(&entries[i].hwaddr, LLADDR(sdl), NEIGH_ETHSIZ);
    }
    i++;
  }

  free(rt);
  *nentries = count;
  return entries;

cleanup_rt:
  free(rt);
fail:
  if (err) {
    *err = errno;
  }
  return NULL;
}

void neigh_free_ip4_entries(struct neigh_ip4_entry *entries) {
  if (entries) {
    free(entries);
  }
}

void neigh_free_ip6_entries(struct neigh_ip6_entry *entries) {
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

struct neigh_ip4_entry *neigh_get_ip4_entries(size_t *nentries, int *err) {
  return NULL;
}

struct neigh_ip6_entry *neigh_get_ip6_entries(size_t *nentries, int *err) {
  return NULL;
}

void neigh_free_ip4_entries(struct neigh_ip4_entry *entries) {

}

void neigh_free_ip6_entries(struct neigh_ip6_entry *entries) {

}

const char *neigh_strerror(int err) {
  return "neigh: NYI";
}

#endif
