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
