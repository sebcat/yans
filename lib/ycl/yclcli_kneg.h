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
