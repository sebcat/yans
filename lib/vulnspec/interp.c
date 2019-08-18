#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib/vulnspec/vulnspec.h"

struct vulnspec_validator {
  jmp_buf errjmp;
  const char *data;
  size_t len;
};

#define bail(obj, err) longjmp((obj)->errjmp, (err))
#define ref(obj, offset) ((void*)((obj)->data + (offset)))

static void check(struct vulnspec_validator *v, size_t offset,
    size_t len) {
  uintptr_t p;

  if (offset < VULNSPEC_HEADER_SIZE) {
    bail(v, VULNSPEC_EINVALID_OFFSET);
  }

  if (offset + len > v->len) {
    bail(v, VULNSPEC_EINVALID_OFFSET);
  }

  p = (uintptr_t)v->data;
  if (p + offset < p) {
    bail(v, VULNSPEC_EINVALID_OFFSET);
  } else if (p + offset + len < p) {
    bail(v, VULNSPEC_EINVALID_OFFSET);
  }
}

static void vstr(struct vulnspec_validator *v,
    const struct vulnspec_cvalue *cval) {
  const char *ptr;

  check(v, cval->value.offset, cval->length);
  ptr = ref(v, cval->value.offset + cval->length - 1);
  if (*ptr != '\0') {
    bail(v, VULNSPEC_EINVALID_NODE);
  }
}

static void vcompar(struct vulnspec_validator *v, size_t offset) {
  const struct vulnspec_compar_node *node;

  check(v, offset, sizeof(struct vulnspec_compar_node));
  node = ref(v, offset);
  vstr(v, &node->vendprod);
}

static void vvulnexpr(struct vulnspec_validator *v, size_t offset);

static void vboolean(struct vulnspec_validator *v, size_t offset) {
  const struct vulnspec_boolean_node *node;

  while (offset > 0) {
    check(v, offset, sizeof(struct vulnspec_boolean_node));
    node = ref(v, offset);
    switch(node->type) {
    case VULNSPEC_AND_NODE:
    case VULNSPEC_OR_NODE:
      break;
    default:
      bail(v, VULNSPEC_EINVALID_NODE);
    }

    vvulnexpr(v, node->value.offset);
    offset = node->next.offset;
  }
}

static void vvulnexpr(struct vulnspec_validator *v, size_t offset) {
  const enum vulnspec_node_type *tptr;

  check(v, offset, sizeof(enum vulnspec_node_type));
  tptr = ref(v, offset);
  switch(*tptr) {
  case VULNSPEC_LT_NODE:
  case VULNSPEC_LE_NODE:
  case VULNSPEC_EQ_NODE:
  case VULNSPEC_GE_NODE:
  case VULNSPEC_GT_NODE:
    vcompar(v, offset);
    break;
  case VULNSPEC_AND_NODE:
  case VULNSPEC_OR_NODE:
    vboolean(v, offset);
    break;
  default:
    bail(v, VULNSPEC_EINVALID_NODE);
  }
}

static void vcve(struct vulnspec_validator *v, size_t offset) {
  const struct vulnspec_cve_node *cve;

  while (offset > 0) {
    check(v, offset, sizeof(struct vulnspec_cve_node));
    cve = ref(v, offset);
    if (cve->type != VULNSPEC_CVE_NODE) {
      bail(v, VULNSPEC_EINVALID_NODE);
    }

    vstr(v, &cve->id);
    vstr(v, &cve->description);
    vvulnexpr(v, cve->vulnexpr.offset);
    offset = cve->next.offset;
  }
}

static void vnode(struct vulnspec_validator *v, size_t offset) {
  const enum vulnspec_node_type *tptr;

  check(v, offset, sizeof(enum vulnspec_node_type));
  tptr = ref(v, offset);
  switch(*tptr) {
  case VULNSPEC_CVE_NODE:
    vcve(v, offset);
    break;
  default:
    bail(v, VULNSPEC_EINVALID_NODE);
    break;
  }
}

static int validate(struct vulnspec_validator *v, const char *data,
    size_t len) {
  int status;

  if (len < VULNSPEC_HEADER_SIZE ||
      memcmp(data, VULNSPEC_HEADER, VULNSPEC_HEADER_SIZE) != 0) {
    return VULNSPEC_EHEADER;
  }

  if ((status = setjmp(v->errjmp)) != 0) {
    goto done;
  }

  v->data = data;
  v->len = len;
  vnode(v, VULNSPEC_HEADER_SIZE);
done:
  return status;
}

static const char *mapfd(int fd, size_t *len) {
  struct stat st;
  int ret;
  const char *data;

  ret = fstat(fd, &st);
  if (ret < 0) {
    return NULL;
  }

#ifdef __FreeBSD__
  data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE|MAP_PREFAULT_READ,
      fd, 0);
#else
  data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE|MAP_POPULATE,
      fd, 0);
#endif
  if (data == MAP_FAILED) {
    return NULL;
  }

  *len = st.st_size;
  return data;
}

void vulnspec_init(struct vulnspec_interp *interp,
    int (*on_match)(struct vulnspec_match *, void *)) {
  interp->data = NULL;
  interp->len = 0;
  interp->on_match = on_match;
}

void vulnspec_unloadfile(struct vulnspec_interp *interp) {
  if (interp->len > 0) {
    munmap((char*)interp->data, interp->len);
  }
}

int vulnspec_load(struct vulnspec_interp *interp, const char *data,
    size_t len) {
  struct vulnspec_validator v;
  int ret;

  ret = validate(&v, data, len);
  if (ret != 0) {
    return ret;
  }

  interp->data = data;
  interp->len = len;
  return 0;
}

int vulnspec_loadfile(struct vulnspec_interp *interp, int fd) {
  const char *data;
  size_t len;
  int ret;

  data = mapfd(fd, &len);
  if (data == NULL) {
    return VULNSPEC_ELOAD;
  }

  ret = vulnspec_load(interp, data, len);
  if (ret < 0) {
    vulnspec_unloadfile(interp);
  }

  return ret;
}

static int ecompar(struct vulnspec_interp *interp, size_t offset) {
  const struct vulnspec_compar_node *node;
  const char *vendprod;
  int cmp;

  node = ref(interp, offset);
  vendprod = ref(interp, node->vendprod.value.offset);
  if (strcmp(vendprod, interp->curr_vendprod) != 0) {
    return 0;
  }

  cmp = vaguever_cmp(&interp->curr_version, &node->version);
  switch (node->type) {
    case VULNSPEC_LT_NODE:
      return cmp < 0;
    case VULNSPEC_LE_NODE:
      return cmp <= 0;
    case VULNSPEC_EQ_NODE:
      return cmp == 0;
    case VULNSPEC_GE_NODE:
      return cmp >= 0;
    case VULNSPEC_GT_NODE:
      return cmp > 0;
    default:
      break;
  }

  return 0;
}

static int evulnexpr(struct vulnspec_interp *interp, size_t offset);

static int eboolean(struct vulnspec_interp *interp, size_t offset) {
  const struct vulnspec_boolean_node *node;
  int ret = 0;

  while (offset > 0) {
    node = ref(interp, offset);
    switch(node->type) {
    case VULNSPEC_AND_NODE:
    case VULNSPEC_OR_NODE:
      break;
    default:
      bail(interp, VULNSPEC_EINVALID_NODE);
    }

    ret = evulnexpr(interp, node->value.offset);
    if (!ret && node->type == VULNSPEC_AND_NODE) {
      break;
    } else if (ret && node->type == VULNSPEC_OR_NODE) {
      break;
    }

    offset = node->next.offset;
  }

  return ret;
}

static int evulnexpr(struct vulnspec_interp *interp, size_t offset) {
  const enum vulnspec_node_type *tptr;
  int ret = 0;

  tptr = ref(interp, offset);
  switch(*tptr) {
  case VULNSPEC_LT_NODE:
  case VULNSPEC_LE_NODE:
  case VULNSPEC_EQ_NODE:
  case VULNSPEC_GE_NODE:
  case VULNSPEC_GT_NODE:
    ret = ecompar(interp, offset);
    break;
  case VULNSPEC_AND_NODE:
  case VULNSPEC_OR_NODE:
    ret = eboolean(interp, offset);
    break;
  default:
    bail(interp, VULNSPEC_EINVALID_NODE);
  }

  return ret;
}

static void enodes(struct vulnspec_interp *interp, size_t offset) {
  const struct vulnspec_cve_node *cve;
  int ret;

  while (offset > 0) {
    cve = ref(interp, offset);
    if (cve->type != VULNSPEC_CVE_NODE) {
      bail(interp, VULNSPEC_EINVALID_NODE);
    }

    ret = evulnexpr(interp, cve->vulnexpr.offset);
    if (ret && interp->on_match) {
      struct vulnspec_match m = {
        .id = ref(interp, cve->id.value.offset),
        .cvss2_base = (float)cve->cvss2_base / 100.0,
        .cvss3_base = (float)cve->cvss3_base / 100.0,
        .desc = ref(interp, cve->description.value.offset),
      };

      ret = interp->on_match(&m, interp->on_match_data);
      if (ret <= 0) {
        bail(interp, ret);
      }
    }
    offset = cve->next.offset;
  }
}

int vulnspec_eval(struct vulnspec_interp *interp, const char *vendprod,
    const char *version, void *data) {
  int status;

  /* inited but no code loaded - do nothing, without error */
  if (interp->len <=
      (VULNSPEC_HEADER_SIZE + sizeof(enum vulnspec_node_type))) {
    return 0;
  }

  /* unset vendprod - do nothing, without error */
  if (!vendprod || !*vendprod) {
    return 0;
  }

  /* unset version - do nothing, without error */
  if (!version || !*version) {
    return 0;
  }

  if ((status = setjmp(interp->errjmp)) != 0) {
    goto done;
  }

  interp->curr_vendprod = vendprod;
  vaguever_init(&interp->curr_version, version);
  interp->on_match_data = data;
  enodes(interp, VULNSPEC_HEADER_SIZE);
done:
  return status;
}
