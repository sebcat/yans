#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <lib/util/macros.h>
#include <lib/match/reset.h>
#include <apps/webfetch/modules/matcher.h>

#define DEFAULT_OUTPATH "-"

struct matcher_data {
  reset_t *reset;
  FILE *out;
};

struct pattern_data {
  enum reset_match_type type;
  const char *name;
  const char *pattern;
};

struct match_data {
  enum reset_match_type type;
  const char *name;
  const char *substring;
};

/* genmatcher output */
#include <apps/webfetch/modules/matcher_httpheader.c>

int matcher_init(struct module_data *mod) {
  struct matcher_data *m;
  size_t i;
  int ret;
  int ch;
  const char *outpath = DEFAULT_OUTPATH;
  char *optstr = "o:";
  struct option opts[] = {
    {"out-matchfile", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0},
  };

  optind = 1; /* reset getopt(_long) state */
  while ((ch = getopt_long(mod->argc, mod->argv, optstr, opts, NULL)) != -1) {
    switch (ch) {
    case 'o':
      outpath = optarg;
      break;
    default:
      goto usage;
    }
  }

  m = calloc(1, sizeof(struct matcher_data));
  if (m == NULL) {
    fprintf(stderr, "failed to allocate matcher data\n");
    goto fail;
  }

  m->reset = reset_new();
  if (m->reset == NULL) {
    fprintf(stderr, "failed to create reset for matcher\n");
    goto fail_free_m;
  }

  for (i = 0; i < ARRAY_SIZE(httpheader_); i++) {
    ret = reset_add_type_name_pattern(m->reset, httpheader_[i].type,
        httpheader_[i].name, httpheader_[i].pattern);
    if (ret == RESET_ERR) {
      fprintf(stderr, "failed to add pattern entry %zu: %s\n", i,
          reset_strerror(m->reset));
      goto fail_reset_free;
    }
  }

  ret = reset_compile(m->reset);
  if (ret != RESET_OK) {
    fprintf(stderr, "failed to compile reset: %s\n",
        reset_strerror(m->reset));
    goto fail_reset_free;
  }

  ret = opener_fopen(mod->opener, outpath, "wb", &m->out);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", outpath, opener_strerror(mod->opener));
    goto fail_reset_free;
  }

  mod->mod_data = m;
  return 0;
fail_reset_free:
  reset_free(m->reset);
fail_free_m:
  free(m);
fail:
  return -1;
usage:
  fprintf(stderr, "usage: matcher [--out-matchfile <path>]\n");
  return -1;
}

static void register_match(struct matcher_data *m,
    struct fetch_transfer *t, struct match_data *match) {
  /* TODO:
   *   If this is a component match, it should be deduplicated by
   * <match type>:<service ID>:<name>:<substring> and written to
   * --out-components
   *
   *   If this is another type of finding, ...? What other types are
   * there? Directory listing should probably be per path, but...
   *
   * The prints below debug stuff for now
   */

  if (match->substring) {
    fprintf(m->out, "MATCH: %d %s %s\n", match->type, match->name,
        match->substring);
  } else {
    fprintf(m->out, "MATCH: %d %s\n", match->type, match->name);
  }
}

void matcher_process(struct fetch_transfer *t, void *data) {
  struct matcher_data *m = data;
  int ret;
  int id;
  const char *header;
  size_t headerlen;
  struct match_data match;

  /* see if any HTTP header patterns match, and if they do register the
   * match */
  headerlen = fetch_transfer_headerlen(t);
  if (headerlen > 0) {
    header = fetch_transfer_header(t);
    ret = reset_match(m->reset, header, headerlen);
    if (ret != RESET_ERR) {
      while ((id = reset_get_next_match(m->reset)) >= 0) {
        match.type = reset_get_type(m->reset, id);
        match.name = reset_get_name(m->reset, id);
        match.substring =
            reset_get_substring(m->reset, id, header, headerlen, NULL);
        register_match(m, t, &match);
      }
    }
  }
}

void matcher_cleanup(void *data) {
  struct matcher_data *m = data;
  if (m) {
    fclose(m->out);
    reset_free(m->reset);
    free(m);
  }
}
