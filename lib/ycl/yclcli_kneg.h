#ifndef YANS_KNEGCLI_H__
#define YANS_KNEGCLI_H__

#include <lib/ycl/yclcli.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define KNEGCLI_DFLPATH LOCALSTATEDIR "/yans/knegd/knegd.sock"

int yclcli_kneg_manifest(struct yclcli_ctx *ctx, char **out);
int yclcli_kneg_queueinfo(struct yclcli_ctx *ctx, char **out);

#endif