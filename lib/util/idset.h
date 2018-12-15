#ifndef UTIL_IDSET_H__
#define UTIL_IDSET_H__

struct idset_ctx;

/* Returns a set for the interval [0, nids), or returns NULL on error. */
struct idset_ctx *idset_new(unsigned int nids);

/* Deallocates 'ctx' */
void idset_free(struct idset_ctx *ctx);

/* Returns the next ID and markes it as used, or -1 on error. */
int idset_use_next(struct idset_ctx *ctx);

/* Marks an ID as unused */
void idset_clear(struct idset_ctx *ctx, int id);

#endif
