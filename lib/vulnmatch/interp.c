#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib/vulnmatch/vulnmatch.h"

struct vulnmatch_validator {
  jmp_buf errjmp;
  const char *data;
  size_t len;
};

#define bail(obj, err) longjmp((obj)->errjmp, (err))
#define ref(obj, offset) ((void*)((obj)->data + (offset)))

static void check(struct vulnmatch_validator *v, size_t offset,
    size_t len) {
  uintptr_t p;

  if (offset < VULNMATCH_HEADER_SIZE) {
    bail(v, VULNMATCH_EINVALID_OFFSET);
  }

  if (offset + len > v->len) {
    bail(v, VULNMATCH_EINVALID_OFFSET);
  }

  p = (uintptr_t)v->data;
  if (p + offset < p) {
    bail(v, VULNMATCH_EINVALID_OFFSET);
  } else if (p + offset + len < p) {
    bail(v, VULNMATCH_EINVALID_OFFSET);
  }
}

static void vstr(struct vulnmatch_validator *v,
    const struct vulnmatch_cvalue *cval) {
  const char *ptr;

  check(v, cval->value.offset, cval->length);
  ptr = ref(v, cval->value.offset + cval->length - 1);
  if (*ptr != '\0') {
    bail(v, VULNMATCH_EINVALID_NODE);
  }
}

static void vcompar(struct vulnmatch_validator *v, size_t offset) {
  const struct vulnmatch_compar_node *node;

  check(v, offset, sizeof(struct vulnmatch_compar_node));
  node = ref(v, offset);
  vstr(v, &node->vendprod);
}

static void vvulnexpr(struct vulnmatch_validator *v, size_t offset);

static void vboolean(struct vulnmatch_validator *v, size_t offset) {
  const struct vulnmatch_boolean_node *node;

  while (offset > 0) {
    check(v, offset, sizeof(struct vulnmatch_boolean_node));
    node = ref(v, offset);
    switch(node->type) {
    case VULNMATCH_AND_NODE:
    case VULNMATCH_OR_NODE:
      break;
    default:
      bail(v, VULNMATCH_EINVALID_NODE);
    }

    vvulnexpr(v, node->value.offset);
    offset = node->next.offset;
  }
}

static void vvulnexpr(struct vulnmatch_validator *v, size_t offset) {
  const enum vulnmatch_node_type *tptr;

  check(v, offset, sizeof(enum vulnmatch_node_type));
  tptr = ref(v, offset);
  switch(*tptr) {
  case VULNMATCH_LT_NODE:
  case VULNMATCH_LE_NODE:
  case VULNMATCH_EQ_NODE:
  case VULNMATCH_GE_NODE:
  case VULNMATCH_GT_NODE:
    vcompar(v, offset);
    break;
  case VULNMATCH_AND_NODE:
  case VULNMATCH_OR_NODE:
    vboolean(v, offset);
    break;
  default:
    bail(v, VULNMATCH_EINVALID_NODE);
  }
}

static void vcve(struct vulnmatch_validator *v, size_t offset) {
  const struct vulnmatch_cve_node *cve;

  while (offset > 0) {
    check(v, offset, sizeof(struct vulnmatch_cve_node));
    cve = ref(v, offset);
    if (cve->type != VULNMATCH_CVE_NODE) {
      bail(v, VULNMATCH_EINVALID_NODE);
    }

    vstr(v, &cve->id);
    vstr(v, &cve->description);
    vvulnexpr(v, cve->vulnexpr.offset);
    offset = cve->next.offset;
  }
}

static void vnode(struct vulnmatch_validator *v, size_t offset) {
  const enum vulnmatch_node_type *tptr;

  check(v, offset, sizeof(enum vulnmatch_node_type));
  tptr = ref(v, offset);
  switch(*tptr) {
  case VULNMATCH_CVE_NODE:
    vcve(v, offset);
    break;
  default:
    bail(v, VULNMATCH_EINVALID_NODE);
    break;
  }
}

static int validate(struct vulnmatch_validator *v, const char *data,
    size_t len) {
  int status;

  if (len < VULNMATCH_HEADER_SIZE ||
      memcmp(data, VULNMATCH_HEADER, VULNMATCH_HEADER_SIZE) != 0) {
    return VULNMATCH_EHEADER;
  }

  if ((status = setjmp(v->errjmp)) != 0) {
    goto done;
  }

  v->data = data;
  v->len = len;
  vnode(v, VULNMATCH_HEADER_SIZE);
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

void vulnmatch_init(struct vulnmatch_interp *interp,
    int (*on_match)(struct vulnmatch_match *, void *)) {
  interp->data = NULL;
  interp->len = 0;
  interp->on_match = on_match;
}

void vulnmatch_unloadfile(struct vulnmatch_interp *interp) {
  if (interp->len > 0) {
    munmap((char*)interp->data, interp->len);
  }
}

int vulnmatch_load(struct vulnmatch_interp *interp, const char *data,
    size_t len) {
  struct vulnmatch_validator v;
  int ret;

  ret = validate(&v, data, len);
  if (ret != 0) {
    return ret;
  }

  interp->data = data;
  interp->len = len;
  return 0;
}

int vulnmatch_loadfile(struct vulnmatch_interp *interp, int fd) {
  const char *data;
  size_t len;
  int ret;

  data = mapfd(fd, &len);
  if (data == NULL) {
    return VULNMATCH_ELOAD;
  }

  ret = vulnmatch_load(interp, data, len);
  if (ret < 0) {
    vulnmatch_unloadfile(interp);
  }

  return ret;
}

static int ecompar(struct vulnmatch_interp *interp, size_t offset) {
  const struct vulnmatch_compar_node *node;
  const char *vendprod;
  int cmp;

  node = ref(interp, offset);
  vendprod = ref(interp, node->vendprod.value.offset);
  if (strcmp(vendprod, interp->curr_vendprod) != 0) {
    return 0;
  }

  cmp = vaguever_cmp(&interp->curr_version, &node->version);
  switch (node->type) {
    case VULNMATCH_LT_NODE:
      return cmp < 0;
    case VULNMATCH_LE_NODE:
      return cmp <= 0;
    case VULNMATCH_EQ_NODE:
      return cmp == 0;
    case VULNMATCH_GE_NODE:
      return cmp >= 0;
    case VULNMATCH_GT_NODE:
      return cmp > 0;
    default:
      break;
  }

  return 0;
}

static int evulnexpr(struct vulnmatch_interp *interp, size_t offset);

static int eboolean(struct vulnmatch_interp *interp, size_t offset) {
  const struct vulnmatch_boolean_node *node;
  int ret = 0;

  while (offset > 0) {
    node = ref(interp, offset);
    switch(node->type) {
    case VULNMATCH_AND_NODE:
    case VULNMATCH_OR_NODE:
      break;
    default:
      bail(interp, VULNMATCH_EINVALID_NODE);
    }

    ret = evulnexpr(interp, node->value.offset);
    if (!ret && node->type == VULNMATCH_AND_NODE) {
      break;
    } else if (ret && node->type == VULNMATCH_OR_NODE) {
      break;
    }

    offset = node->next.offset;
  }

  return ret;
}

static int evulnexpr(struct vulnmatch_interp *interp, size_t offset) {
  const enum vulnmatch_node_type *tptr;
  int ret = 0;

  tptr = ref(interp, offset);
  switch(*tptr) {
  case VULNMATCH_LT_NODE:
  case VULNMATCH_LE_NODE:
  case VULNMATCH_EQ_NODE:
  case VULNMATCH_GE_NODE:
  case VULNMATCH_GT_NODE:
    ret = ecompar(interp, offset);
    break;
  case VULNMATCH_AND_NODE:
  case VULNMATCH_OR_NODE:
    ret = eboolean(interp, offset);
    break;
  default:
    bail(interp, VULNMATCH_EINVALID_NODE);
  }

  return ret;
}

static void enodes(struct vulnmatch_interp *interp, size_t offset) {
  const struct vulnmatch_cve_node *cve;
  int ret;

  while (offset > 0) {
    cve = ref(interp, offset);
    if (cve->type != VULNMATCH_CVE_NODE) {
      bail(interp, VULNMATCH_EINVALID_NODE);
    }

    ret = evulnexpr(interp, cve->vulnexpr.offset);
    if (ret && interp->on_match) {
      struct vulnmatch_match m = {
        .id = ref(interp, cve->id.value.offset),
	.cvss3_base = (float)cve->cvss3_base / 100.0,
	.desc = ref(interp, cve->id.value.offset),
      };

      ret = interp->on_match(&m, interp->on_match_data);
      if (ret <= 0) {
        bail(interp, ret);
      }
    }
    offset = cve->next.offset;
  }
}

int vulnmatch_eval(struct vulnmatch_interp *interp, const char *vendprod,
    const char *version, void *data) {
  int status;

  /* inited but no code loaded - do nothing without error */
  if (interp->len <=
      (VULNMATCH_HEADER_SIZE + sizeof(enum vulnmatch_node_type))) {
    return 0;
  }

  if ((status = setjmp(interp->errjmp)) != 0) {
    goto done;
  }

  interp->curr_vendprod = vendprod;
  vaguever_init(&interp->curr_version, version);
  interp->on_match_data = data;
  enodes(interp, VULNMATCH_HEADER_SIZE);
done:
  return status;
}
