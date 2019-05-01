#ifndef WEBFETCH_WRITER_H__
#define WEBFETCH_WRITER_H__

#include <apps/webfetch/module.h>
#include <apps/webfetch/fetch.h>

int writer_init(struct module_data *mod);
void writer_process(struct fetch_transfer *t, void *data);
void writer_cleanup(void *data);

#endif
