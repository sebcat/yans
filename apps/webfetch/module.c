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
#include <stddef.h>
#include <string.h>

#include <lib/util/macros.h>
#include <apps/webfetch/module.h>
#include <apps/webfetch/modules/writer.h>
#include <apps/webfetch/modules/logger.h>
#include <apps/webfetch/modules/matcher.h>

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
  },
  {
    .name        = "writer",
    .mod_init    = writer_init,
    .mod_process = writer_process,
    .mod_cleanup = writer_cleanup,
  },
  {
    .name        = "matcher",
    .mod_init    = matcher_init,
    .mod_process = matcher_process,
    .mod_cleanup = matcher_cleanup,
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
