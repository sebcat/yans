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
