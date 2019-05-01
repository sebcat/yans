#ifndef WEBFETCH_MODULE_H__
#define WEBFETCH_MODULE_H__

#include <lib/ycl/opener.h>
#include <apps/webfetch/fetch.h>

struct module_data {
  char *name;
  int argc;
  char **argv;
  struct opener_ctx *opener;

  void *mod_data;
  int (*mod_init)(struct module_data *);
  void (*mod_process)(struct fetch_transfer *, void *);
  void (*mod_cleanup)(void *);
};

int module_load(struct module_data *mod);

static inline void module_cleanup(struct module_data *mod) {
  if (mod && mod->mod_cleanup) {
    mod->mod_cleanup(mod->mod_data);
  }
}

#endif
