#ifndef WEBFETCH_MATCHER_H__
#define WEBFETCH_MATCHER_H__

#include <apps/webfetch/module.h>
#include <apps/webfetch/fetch.h>

int matcher_init(struct module_data *mod);
void matcher_process(struct fetch_transfer *t, void *data);
void matcher_cleanup(void *data);

#endif
