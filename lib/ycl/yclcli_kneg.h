#ifndef YANS_KNEGCLI_H__
#define YANS_KNEGCLI_H__

#include <lib/ycl/yclcli.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define KNEGCLI_DFLPATH LOCALSTATEDIR "/yans/knegd/knegd.sock"

int yclcli_kneg_manifest(struct yclcli_ctx *ctx, char **out);
int yclcli_kneg_queueinfo(struct yclcli_ctx *ctx, char **out);
int yclcli_kneg_status(struct yclcli_ctx *ctx, const char *id,
    size_t idlen, char **out);
int yclcli_kneg_queue(struct yclcli_ctx *ctx, const char *id,
    const char *type, const char *name, long timeout, char **out);

#endif
