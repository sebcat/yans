#ifndef RESET_H__
#define RESET_H__

#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RESET_OK   0
#define RESET_ERR -1

enum reset_match_type {
  RESET_MATCH_UNKNOWN       = 0,
  RESET_MATCH_COMPONENT     = 1,
  RESET_MATCH_MAX,
};

typedef struct reset_t reset_t;

struct reset_pattern {
  enum reset_match_type type;
  const char *name;
  const char *pattern;
};

const char *reset_type2str(enum reset_match_type t);

/* return RESET_ERR and sets the error string */
int reset_error(reset_t *reset, const char *errstr);

const char *reset_strerror(reset_t *reset);

reset_t *reset_new();
void reset_free(reset_t *reset);

int reset_add_pattern(reset_t *reset, const char *re);
int reset_add_name_pattern(reset_t *reset, const char *name, const char *re);
int reset_add_type_name_pattern(reset_t *reset, enum reset_match_type mtype,
    const char *name, const char *re);

int reset_compile(reset_t *reset);
int reset_match(reset_t *reset, const char *data, size_t datalen);
int reset_get_next_match(reset_t *reset);
const char *reset_get_name(reset_t *reset, int id);
enum reset_match_type reset_get_type(reset_t *reset, int id);
const char *reset_get_substring(reset_t *reset, int id, const char *data,
    size_t len, size_t *ol);

/* add + compile of multiple patterns */
int reset_load(reset_t *reset, const struct reset_pattern *pattern,
    size_t npatterns);



#ifdef __cplusplus
}
#endif

#endif
