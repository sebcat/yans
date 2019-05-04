#ifndef MATCH_PM_H__
#define MATCH_PM_H__

#include <stdio.h>
#include <lib/match/reset.h>

enum pm_match_type {
  PM_MATCH_UNKNOWN       = 0,
  PM_MATCH_COMPONENT     = 1,
  PM_MATCH_MAX,
};

struct pm_pattern {
  char *name;
};

struct pm_ctx {
  reset_t *reset;
  struct pm_pattern *patterns;
  unsigned int plen;
  unsigned int pcap;
};

int pm_init(struct pm_ctx *ctx);
void pm_cleanup(struct pm_ctx *ctx);
int pm_add_pattern(struct pm_ctx *ctx, enum pm_match_type t,
    const char *name, const char *pattern);
int pm_load_csv(struct pm_ctx *ctx, FILE *in, size_t *npatterns);

#endif
