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
#ifndef YANS_VAGUEVER_H__
#define YANS_VAGUEVER_H__

/* a lot of different versioning schemes exists, and they differ in subtle
 * ways. This code is an attempt to provide a version parser that can be
 * used for semver-like versioning schemes as a fallback. */

enum vaguever_fields {
  VAGUEVER_MAJOR = 0,
  VAGUEVER_MINOR,
  VAGUEVER_PATCH,
  VAGUEVER_BUILD,
  VAGUEVER_NFIELDS
};

struct vaguever_version {
  int nused;
  int fields[VAGUEVER_NFIELDS];
};

void vaguever_init(struct vaguever_version *v, const char *str);
void vaguever_str(struct vaguever_version *v, char *out, size_t len);
int vaguever_cmp(const struct vaguever_version *v1,
    const struct vaguever_version *v2);

#endif
