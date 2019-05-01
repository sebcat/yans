#ifndef WEBFETCH_SIMPLELOG_H__
#define WEBFETCH_SIMPLELOG_H__

#include <apps/webfetch/module.h>
#include <apps/webfetch/fetch.h>

int simplelog_init(struct module_data *mod);
void simplelog_process(struct fetch_transfer *t, void *data);
void simplelog_cleanup(void *data);

#endif
