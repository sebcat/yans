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
#ifndef UTIL_FLAGSET_H__
#define UTIL_FLAGSET_H__

/* maximum length of a flag string */
#define FLAGSET_MAXSZ 31

/* valid flag separators */
#define FLAGSET_SEPS "\r\n\t |,"

struct flagset_map {
  const char *name;
  size_t namelen;
  unsigned int flag;
};

#define FLAGSET_ENTRY(name__, flag__) \
    {.name = (name__), .namelen = sizeof((name__)) - 1, .flag = (flag__)}

#define FLAGSET_END {0}

struct flagset_result {
  unsigned int flags;
  const char *errmsg;
  size_t erroff;
};

/* returns -1 on error, 0 on success. Fills in flagset_result accordingly */
int flagset_from_str(const struct flagset_map *m, const char *s,
    struct flagset_result *out);


#endif /* UTIL_FLAGSET_H__ */
