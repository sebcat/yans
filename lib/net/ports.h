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
#ifndef NET_PORTS_H__
#define NET_PORTS_H__

#include <stdint.h>
#include <stddef.h>

#include <lib/util/buf.h>
#include <lib/util/reorder.h>

/*
 * 0) port lists are inclusive, e.g., 1-2 means port 1 and 2
 * 1) port lists should be sorted: 3-4,1 -> 1,3-4
 * 2) overlapping/adjacent groups should be joined: 3-4,1-2 -> 1-4
 */

struct port_range {
  uint16_t start;
  uint16_t end;
};

struct port_ranges {
  size_t cap;                 /* number of allocated struct port_range's */
  size_t nranges;             /* number of used struct port_range's */
  struct port_range *ranges;

  /* curr_* - for iteration */
  size_t curr_range;
  uint16_t curr_port;

  uint16_t flags; /* reserved for internal use */
};

struct port_r4ranges {
  struct port_ranges *rs;
  size_t nranges;
  struct reorder32 *ranges;
  int *rangemap;
  size_t mapindex;
};

/* reset range iteration */
#define port_ranges_reset(rs) \
  (rs)->curr_range = 0;       \
  (rs)->curr_port = 0;

int port_ranges_from_str(struct port_ranges *rs, const char *s,
    size_t *fail_off);
int port_ranges_to_buf(struct port_ranges *rs, buf_t *buf);
void port_ranges_cleanup(struct port_ranges *rs);
int port_ranges_next(struct port_ranges *rs, uint16_t *out);
int port_ranges_add_ranges(struct port_ranges *dst, struct port_ranges *from);
int port_ranges_add_range(struct port_ranges *dst, struct port_range *r);
int port_ranges_add_port(struct port_ranges *dst, uint16_t port);

/* initializes a reordered iterator of port_ranges. port_ranges must be
 * alive for as long as port_r4ranges is used */
int port_r4ranges_init(struct port_r4ranges *r4, struct port_ranges *rs);
void port_r4ranges_cleanup(struct port_r4ranges *r4);
int port_r4ranges_next(struct port_r4ranges *r4, uint16_t *out);

#endif /* NET_PORTS_H__ */
