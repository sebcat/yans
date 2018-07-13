#ifndef UTIL_STR_H__
#define UTIL_STR_H__

/* map 'func' to the substrings in s separated by any character(s) in 'seps' */
int str_map_field(const char *s, size_t len,
    const char *seps, size_t sepslen,
    int (*func)(const char *, size_t, void *),
    void *data);

/* like str_map_field, but for \0-terminated strings */
int str_map_fieldz(const char *s, const char *seps,
    int (*func)(const char *, size_t, void *),
    void *data);



#endif
