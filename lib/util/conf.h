#ifndef UTIL_CONF_H_
#define UTIL_CONF_H_

#include <stddef.h>

#define CONF_ERRBUFSZ 128

struct conf {
  /* -- internal fields -- */
  int flags;
  char *data;
  size_t datalen;
  char errbuf[CONF_ERRBUFSZ];
};

enum conf_type {
  CONF_TSTR,
  CONF_TULONG,
};

struct conf_map {
  const char *key;
  size_t offset;
  enum conf_type cfgtype;
};

/* macro for defining conf_map entries */
#define CONF_MENTRY(__type, __member, __cfgtype) \
  {.key = #__member,                             \
   .offset = offsetof(__type, __member),         \
   .cfgtype = __cfgtype}

/* macro for defining the end of a netstring_map table */
#define CONF_MENTRY_END {0}

/*

  CONF_MENTRY and CONF_MENTRY_END should be used as follows:

  struct myconf {
    const char *some_setting;
    const char *another_setting;
    unsigned long third_setting;
  };

  struct conf_map m[] = {
    CONF_MENTRY(struct myconf, some_setting, CONF_TSTR),
    CONF_MENTRY(struct myconf, another_setting, CONF_TSTR),
    CONF_MENTRY(struct myconf, third_setting, CONF_TULONG),
    CONF_MENTRY_END,
  };
*/

/* conf_init --
 *   open a configuration file and load it to memory. Returns -1 on error. */
int conf_init(struct conf *cfg, const char *path);

/* conf_init_from_str --
 *   copy a string to the conf. Returns -1 on error. */
int conf_init_from_str(struct conf *cfg, const char *str);

/* conf_cleanup --
 *   free the memory associated with a configuration. Any pointers
 *   from conf_parse will become invalid */
void conf_cleanup(struct conf *cfg);

/* conf_parse --
 *   parse the configuration to a struct defined by the configuration map.
 *   Pointers set in the resulting struct is valid until conf_free is called
 *   on the configuration context. Returns -1 on error */
int conf_parse(struct conf *cfg, struct conf_map *map, void *out);

/* conf_strerror
 *   returns a (possibly empty) string describing an error */
static inline const char *conf_strerror(struct conf *cfg) {
  return cfg->errbuf;
}


#endif /* UTIL_CONF_H_ */
