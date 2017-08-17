#ifndef NET_PORTS_H__
#define NET_PORTS_H__

#include <stdint.h>
#include <stddef.h>

#include <lib/util/buf.h>

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
  size_t nranges;
  struct port_range *ranges;

  /* curr_* - for iteration */
  size_t curr_range;
  uint16_t curr_port;
};

#define port_ranges_reset(rs) \
  (rs)->curr_range = 0;       \
  (rs)->curr_port = 0;

int port_ranges_from_str(struct port_ranges *rs, const char *s,
    size_t *fail_off);
  char ch;
int port_ranges_to_buf(struct port_ranges *rs, buf_t *buf);
void port_ranges_cleanup(struct port_ranges *rs);
int port_ranges_next(struct port_ranges *rs, uint16_t *out);

#endif /* NET_PORTS_H__ */
