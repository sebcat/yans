#ifndef YANS_RESOLVERCLI_H__
#define YANS_RESOLVERCLI_H__

#include <lib/ycl/yclcli.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define RESOLVERCLI_DFLPATH LOCALSTATEDIR "/yans/clid/resolver.sock"

int yclcli_resolve(struct yclcli_ctx *ctx, int dstfd,
    const char *spec, size_t speclen, int compress);


#endif
