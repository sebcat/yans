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
#include <stdlib.h>

#include <lib/util/macros.h>
#include <lib/match/component.h>

#define INITIAL_CTBL_SIZE 64

#define COMPONENT_FSORTED (1 << 0)

static struct component_entry *component_entry_new(const char *name, const char *version,
    int component_id) {
  struct component_entry *e;

  e = calloc(1, sizeof(struct component_entry));
  if (!e) {
    return NULL;
  }

  e->id = component_id;
  if (name) {
    e->name = strdup(name);
    if (!e->name) {
      goto free_e;
    }
  }

  if (version) {
    e->version = strdup(version);
    if (!e->version) {
      goto free_name;
    }
  }

  return e;
free_name:
  free((void*)e->name);
free_e:
  free(e);
  return NULL;
}

static void component_entry_free(struct component_entry *e) {
  if (e) {
    free((void*)e->name);
    free((void*)e->version);
    free(e->services);
    free(e);
  }
}

static int component_entry_has_service(struct component_entry *e, int service) {
  int i;

  /* n in O(n) is usually small, right? */
  for (i = 0; i < e->slen; i++) {
    if (e->services[i] == service) {
      break;
    }
  }

  return i == e->slen ? 0 : 1;
}


static objtbl_hash_t ctblhash(const void *obj, objtbl_hash_t seed) {
  const struct component_entry *e = obj;
  return objtbl_strhash(e->version, objtbl_strhash(e->name, seed));
}

static int ctblcmp(const void *key, const void *entry) {
  const struct component_entry *l = key;
  const struct component_entry *r = entry;
  int cmp;

  NULLCMP(l->name, r->name);
  cmp = strcmp(l->name, r->name);
  if (cmp == 0) {
    NULLCMP(l->version, r->version); /* TODO: NULL vs. empty string? */
    cmp = strcmp(l->version, r->version);
  }

  return cmp;
}

int component_init(struct component_ctx *c) {
  int ret;
  static const struct objtbl_opts ctbl_opts = {
    .hashseed = 666, /* TODO: Improve seeding */
    .hashfunc = ctblhash,
    .cmpfunc  = ctblcmp,
  };

  c->flags = 0;
  c->component_id = 1;
  ret = objtbl_init(&c->ctbl, &ctbl_opts, INITIAL_CTBL_SIZE);
  if (ret != OBJTBL_OK) {
    return -1;
  }

  return 0;
}

static int clear_component_entry(void *data, void *value) {
  (void)data;
  component_entry_free(value);
  return 1;
}

void component_cleanup(struct component_ctx *c) {
  objtbl_foreach(&c->ctbl, clear_component_entry, NULL);
  objtbl_cleanup(&c->ctbl);
}

static int grow(struct component_entry *e) {
  int ncap;
  int nbytes;
  int *ndata;

  ncap = MAX(e->scap / 2, 8) + e->scap;
  nbytes = ncap * sizeof(int);
  if (nbytes <= e->scap) {
    return -1; /* overflow */
  }

  ndata = realloc(e->services, (size_t)nbytes);
  if (ndata == NULL) {
    return -1;
  }

  e->scap = ncap;
  e->services = ndata;
  return 0;
}

static int component_entry_add_service(struct component_entry *e, int service) {
  int ret;

  if (e->slen == e->scap) {
    ret = grow(e);
    if (ret < 0) {
      return -1;
    }
  }

  e->services[e->slen] = service;
  e->slen++;
  return 0;
}

int component_register(struct component_ctx *c, const char *name,
    const char *version, int service_id) {
  struct component_entry key;
  struct component_entry *obj;
  void *valptr = NULL;
  int ret;

  /* component_foreach was called, making it impossible to insert new
   * values */
  if (c->flags & COMPONENT_FSORTED) {
    return -1;
  }

  /* get/insert component from table */
  key.name = name;
  key.version = version;
  ret = objtbl_get(&c->ctbl, &key, &valptr);
  if (ret == OBJTBL_ENOTFOUND) {
    obj = component_entry_new(name, version, c->component_id);
    if (obj == NULL) {
      return -1;
    }

    ret = objtbl_insert(&c->ctbl, obj);
    if (ret != OBJTBL_OK) {
      component_entry_free(obj);
      return -1;
    }

    c->component_id++;
  } else {
    obj = valptr;
  }

  if (!component_entry_has_service(obj, service_id)) {
    component_entry_add_service(obj, service_id);
  }

  return 0;
}

static int component_idcmp(const void *a, const void *b) {
  const struct component_entry *left = a;
  const struct component_entry *right = b;
  NULLCMP(left, right);
  return (int)((unsigned int)(right->id-1) - (unsigned int)(left->id-1));
}

int component_foreach(struct component_ctx *c,
    int (*func)(void *, void *),
    void *data) {

  if (!(c->flags & COMPONENT_FSORTED)) {
    objtbl_set_cmpfunc(&c->ctbl, component_idcmp);
    objtbl_sort(&c->ctbl);
    c->flags |= COMPONENT_FSORTED;
  }

  return objtbl_foreach(&c->ctbl, func, data);
}
