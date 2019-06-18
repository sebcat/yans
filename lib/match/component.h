#ifndef MATCH_COMPONENT_H__
#define MATCH_COMPONENT_H__

#include <lib/util/objtbl.h>

struct component_entry {
  int id;
  const char *name;
  const char *version;

  int *services;
  int slen; /* # of services registered */
  int scap; /* capacity of 'services' */
};

struct component_ctx {
  int flags;
  int component_id; /* next component ID to assign */
  struct objtbl_ctx ctbl;
};

int component_init(struct component_ctx *c);
void component_cleanup(struct component_ctx *c);
int component_register(struct component_ctx *c, const char *name,
    const char *version, int service_id);

/* XXX: destructive call, _register is not allowed afterwards */
int component_foreach(struct component_ctx *c,
    int (*func)(void *, void *),
    void *data);


#endif
