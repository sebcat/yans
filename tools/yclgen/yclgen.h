#ifndef YCLGEN_H__
#define YCLGEN_H__

#include <stdio.h>

#define MAX_NAMELEN 32
#define MAX_FIELDS 32

#define MSGF_HASDARR (1 << 0)
#define MSGF_HASLARR (1 << 1)
#define MSGF_HASLONG (1 << 2)
#define MSGF_HASDATA (1 << 3)

/* intdicated that the first element is a struct */
#define MSGF_HASNEST (1 << 4)

enum yclgen_field_type {
  FT_DATAARR,
  FT_LONGARR,
  FT_DATA,
  FT_LONG,
};

struct yclgen_field {
  enum yclgen_field_type typ;
  char name[MAX_NAMELEN];
};

struct yclgen_msg {
  struct yclgen_msg *next;
  int flags;
  char name[MAX_NAMELEN];
  int nfields;
  struct yclgen_field fields[MAX_FIELDS];
};

struct yclgen_ctx {
  struct yclgen_msg *latest_msg;
  int flags;
};

int yclgen_parse(struct yclgen_ctx *ctx, FILE *fp);
void yclgen_cleanup(struct yclgen_ctx *ctx);

#endif /* YCLGEN_H__ */
