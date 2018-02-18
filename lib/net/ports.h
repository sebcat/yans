#ifndef NET_PORTS_H__
#define NET_PORTS_H__

#include <stdint.h>
#include <stddef.h>

#include <lib/util/buf.h>
#include <lib/util/reorder.h>

/*
 * 0) port lists are inclusive, e.g., 1-2 means port 1 and 2
 * 1) overlapping/adjacent groups should be joined: 3-4,1-2 -> 1-4
 * 2) port lists should be sorted: 3-4,1 -> 1,3-4
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
int port_ranges_add(struct port_ranges *dst, struct port_ranges *from);

/* initializes a reordered iterator of port_ranges. port_ranges must be
 * alive for as long as port_r4ranges is used */
int port_r4ranges_init(struct port_r4ranges *r4, struct port_ranges *rs);
void port_r4ranges_cleanup(struct port_r4ranges *r4);
int port_r4ranges_next(struct port_r4ranges *r4, uint16_t *out);

#endif /* NET_PORTS_H__ */
