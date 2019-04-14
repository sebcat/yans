#ifndef YANS_STORECLI_H__
#define YANS_STORECLI_H__

#include <lib/ycl/yclcli.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define STORECLI_DFLPATH LOCALSTATEDIR "/yans/stored/stored.sock"

int yclcli_store_enter(struct yclcli_ctx *ctx, const char *id,
    const char **out_id);
int yclcli_store_open(struct yclcli_ctx *ctx, const char *path, int flags,
    int *outfd);
int yclcli_store_fopen(struct yclcli_ctx *ctx, const char *path,
    const char *mode, FILE **outfp);
int yclcli_store_list(struct yclcli_ctx *ctx, const char *id,
    const char *must_match, const char **result, size_t *resultlen);
int yclcli_store_index(struct yclcli_ctx *ctx, size_t before, size_t nelems,
    int *outfd);
int yclcli_store_rename(struct yclcli_ctx *ctx, const char *from,
    const char *to);


#endif
