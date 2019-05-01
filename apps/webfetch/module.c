#include <stddef.h>
#include <string.h>

#include <lib/util/macros.h>
#include <apps/webfetch/module.h>
#include <apps/webfetch/modules/logger.h>

static const struct module_data modules_[] = {
  {
    .name        = "nop",
    .mod_init    = NULL,
    .mod_process = NULL,
    .mod_cleanup = NULL,
  },
  {
    .name        = "logger",
    .mod_init    = logger_init,
    .mod_process = logger_process,
    .mod_cleanup = logger_cleanup,
  }
};

int module_load(struct module_data *mod) {
  int i;

  if (mod->name == NULL || *mod->name == '\0') {
    return -1;
  }

  for (i = 0; i < ARRAY_SIZE(modules_); i++) {
    if (strcmp(mod->name, modules_[i].name) == 0) {
      mod->mod_init = modules_[i].mod_init;
      mod->mod_process = modules_[i].mod_process;
      mod->mod_cleanup = modules_[i].mod_cleanup;
      if (mod->mod_init) {
        return mod->mod_init(mod);
      } else {
        return 0;
      }
    }
  }

  return -1;
}
