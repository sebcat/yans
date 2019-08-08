#include "lib/vulnmatch/vulnmatch.h"

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

int vulnmatch_validate(struct vulnmatch_validator *v, const char *data,
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
