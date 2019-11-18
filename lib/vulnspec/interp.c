/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include "lib/vulnspec/vulnspec.h"
#include "lib/util/nalphaver.h"

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
  if (node->vtype != VULNSPEC_VVAGUE) {
    vstr(v, &node->version.cval);
  }
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

void vulnspec_init(struct vulnspec_interp *interp) {
  memset(interp, 0, sizeof(*interp));
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
  const char *version;
  int cmp;

  node = ref(interp, offset);
  vendprod = ref(interp, node->vendprod.value.offset);
  if (strcmp(vendprod, interp->params.cve_vendprod) != 0) {
    return 0;
  }

  switch (node->vtype) {
  case VULNSPEC_VVAGUE:
    cmp = vaguever_cmp(&interp->params.cve_vaguever_version, &node->version.vague);
    break;
  case VULNSPEC_VNALPHA:
    version = ref(interp, node->version.cval.value.offset);
    /* NB: Assume VULNSPEC_VNALPHA, as that's the only non-VULNSPEC_VVAGUE
     * option ATM */
    cmp = nalphaver_cmp(interp->params.cve_version, version);
    break;
  default:
    return 0;
  }

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

    /* Do not bother evaluating if we have no vendprod or version */
    if (interp->params.cve_version == NULL ||
        !*interp->params.cve_version ||
        interp->params.cve_vendprod == NULL ||
        !*interp->params.cve_vendprod) {
      return;
    }

    ret = evulnexpr(interp, cve->vulnexpr.offset);
    if (ret && interp->params.cve_on_match) {
      struct vulnspec_cve_match m = {
        .id = ref(interp, cve->id.value.offset),
        .cvss2_base = (float)cve->cvss2_base / 100.0,
        .cvss3_base = (float)cve->cvss3_base / 100.0,
        .desc = ref(interp, cve->description.value.offset),
      };

      ret = interp->params.cve_on_match(&m,
          interp->params.cve_on_match_data);
      if (ret <= 0) {
        bail(interp, ret);
      }
    }
    offset = cve->next.offset;
  }
}

int vulnspec_eval(struct vulnspec_interp *interp) {
  int status;

  /* inited but no code loaded - do nothing, without error */
  if (interp->len <=
      (VULNSPEC_HEADER_SIZE + sizeof(enum vulnspec_node_type))) {
    return 0;
  }

  if ((status = setjmp(interp->errjmp)) != 0) {
    goto done;
  }

  enodes(interp, VULNSPEC_HEADER_SIZE);
done:
  return status;
}

void vulnspec_set(struct vulnspec_interp *interp, enum vulnspec_ptype t,
    ...) {
  va_list ap;
  const char *cp;

  va_start(ap, t);
  switch(t) {
  case VULNSPEC_PCVEONMATCH:
    interp->params.cve_on_match =
        va_arg(ap, int(*)(struct vulnspec_cve_match *, void *));
    break;
  case VULNSPEC_PCVEONMATCHDATA:
    interp->params.cve_on_match_data = va_arg(ap, void *);
    break;
  case VULNSPEC_PCVEVENDPROD:
    interp->params.cve_vendprod = va_arg(ap, const char *);
    break;
  case VULNSPEC_PCVEVERSION:
    cp = va_arg(ap, const char *);
    interp->params.cve_version = cp;
    vaguever_init(&interp->params.cve_vaguever_version, cp);
    break;
  default:
    break;
  }

  va_end(ap);
}
