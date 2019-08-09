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

static inline void bail(struct vulnmatch_validator *v, int err) {
  longjmp(v->errjmp, err);
}

static inline const void *ref(struct vulnmatch_validator *v,
    size_t offset) {
  return v->data + offset;
}

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
  vstr(v, &node->version);
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

  if ((status = setjmp(v->errjmp)) != 0) {
    goto done;
  }

  v->data = data;
  v->len = len;
  vnode(v, VULNMATCH_HEADER_SIZE);
done:
  return status;
}

static const char *loadfile(const char *file, size_t *len) {
  struct stat st;
  int ret;
  int fd;
  const char *data;


  fd = open(file, O_RDONLY);
  if (fd < 0) {
    return NULL;
  }

  ret = fstat(fd, &st);
  if (ret < 0) {
    close(fd);
    return NULL;
  }

#ifdef __FreeBSD__
  data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE|MAP_PREFAULT_READ,
      fd, 0);
#else
  data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE|MAP_POPULATE,
      fd, 0);
#endif
  close(fd);
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
  munmap((char*)interp->data, interp->len);
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

int vulnmatch_loadfile(struct vulnmatch_interp *interp, const char *file) {
  const char *data;
  size_t len;
  int ret;

  data = loadfile(file, &len);
  if (data == NULL) {
    return VULNMATCH_ELOAD;
  }

  ret = vulnmatch_load(interp, data, len);
  if (ret < 0) {
    vulnmatch_unloadfile(interp);
  }

  return ret;
}
