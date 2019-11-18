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
