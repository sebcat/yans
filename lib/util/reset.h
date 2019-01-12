#ifndef RESET_H__
#define RESET_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RESET_OK   0
#define RESET_ERR -1

typedef struct reset_t reset_t;

reset_t *reset_new();
void reset_free(reset_t *reset);
const char *reset_strerror(reset_t *reset);

int reset_add(reset_t *reset, const char *re);
int reset_compile(reset_t *reset);
int reset_match(reset_t *reset, const char *data, size_t datalen);
int reset_get_next_match(reset_t *reset);

#ifdef __cplusplus
}
#endif

#endif
