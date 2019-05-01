#ifndef WEBFETCH_SIMPLELOG_H__
#define WEBFETCH_SIMPLELOG_H__

#include <apps/webfetch/module.h>
#include <apps/webfetch/fetch.h>

int logger_init(struct module_data *mod);
void logger_process(struct fetch_transfer *t, void *data);
void logger_cleanup(void *data);

#endif
