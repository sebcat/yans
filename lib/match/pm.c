#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <lib/match/pm.h>
#include <lib/util/csv.h>
#include <lib/util/macros.h>

#define MIN_GROWTH 8

int pm_init(struct pm_ctx *ctx) {
  ctx->patterns = NULL;
  ctx->plen = 0;
  ctx->pcap = 0;
  ctx->reset = reset_new();
  if (ctx->reset == NULL) {
    return -1;
  }

  return 0;
}

void pm_cleanup(struct pm_ctx *ctx) {
  size_t i;

  reset_free(ctx->reset);
  ctx->reset = NULL;
  for (i = 0; i < ctx->plen; i++) {
    free(ctx->patterns[i].name);
  }

  free(ctx->patterns);
}

static int grow_patterns(struct pm_ctx *ctx) {
  struct pm_pattern *newpat;
  unsigned int newcap;
  unsigned int incr;

  incr = MAX(ctx->pcap / 2, MIN_GROWTH);
  newcap = ctx->pcap + incr;
  if (newcap < ctx->pcap || newcap > UINT_MAX/sizeof(struct pm_pattern)) {
    return -1; /* overflow */
  }

  newpat = realloc(ctx->patterns, newcap * sizeof(struct pm_pattern));
  if (newpat == NULL) {
    return -1;
  }

  ctx->patterns = newpat;
  ctx->pcap = newcap;
  return 0;
}

int pm_add_pattern(struct pm_ctx *ctx, enum pm_match_type t,
    const char *name, const char *pattern) {
  int ret;
  char *namedup;

  if (t <= PM_MATCH_UNKNOWN || t >= PM_MATCH_MAX) {
    return -1;
  }

  if (ctx->plen == ctx->pcap) {
    ret = grow_patterns(ctx);
    if (ret < 0) {
      return -1;
    }
  }

  namedup = strdup(name);
  if (namedup == NULL) {
    return -1;
  }

  ret = reset_add(ctx->reset, pattern);
  if (ret == RESET_ERR) {
    goto free_namedup;
  }

  assert(ret == ctx->plen); /* the returned ID must be the pattern index */
  ctx->patterns[ctx->plen].name = namedup;
  ctx->plen++;
  return 0;
free_namedup:
  free(namedup);
  return -1;
}

int pm_load_csv(struct pm_ctx *ctx, FILE *in, size_t *npatterns) {
  struct csv_reader reader;
  int ret;
  int status = 0;
  const char *type;
  const char *name;
  const char *pattern;
  size_t nloaded = 0;
  char *cptr;
  enum pm_match_type mtype;
  

  ret = csv_reader_init(&reader);
  if (ret < 0) {
    return -1;
  }

  /* NB: This is best-effort - There may be empty lines in the CSV,
         it is likely to have column names on the first row, &c. We
         just ignore row parse errors and communicate number of patterns
         added using npatterns instead */
  while (!feof(in)) {
    ret = csv_read_row(&reader, in);
    if (ret < 0) {
      status = -1;
      break;
    }

    type = csv_reader_elem(&reader, 0);
    name = csv_reader_elem(&reader, 1);
    pattern = csv_reader_elem(&reader, 2);
    if (!type || !name || !pattern) {
      continue;
    }

    mtype = (int)strtol(type, &cptr, 10);
    if (*cptr != '\0') {
      continue;
    } 

    ret = pm_add_pattern(ctx, mtype, name, pattern);
    if (ret == 0) {
      nloaded++;
    }
  }

  if (npatterns) {
    *npatterns = nloaded;
  }

  csv_reader_cleanup(&reader);
  return status;
}
