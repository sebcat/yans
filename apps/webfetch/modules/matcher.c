#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <lib/util/macros.h>
#include <lib/util/csv.h>
#include <lib/match/reset.h>
#include <lib/match/component.h>
#include <apps/webfetch/modules/matcher.h>

#define DEFAULT_OUTPATH "-"

struct matcher_data {
  reset_t *reset;
  struct component_ctx ctbl;
  FILE *out_components;
  FILE *out_compsvclist;
  buf_t rowbuf;
};

struct pattern_data {
  enum reset_match_type type;
  const char *name;
  const char *pattern;
};

/* genmatcher output */
#include <apps/webfetch/modules/matcher_httpheader.c>

int matcher_init(struct module_data *mod) {
  struct matcher_data *m;
  size_t i;
  int ret;
  int ch;
  const char *out_components_path = NULL;
  const char *out_compsvclist_path = NULL;
  char *optstr = "o:c:";
  struct option opts[] = {
    {"out-components", required_argument, NULL, 'o'},
    {"out-compsvclist", required_argument, NULL, 'c'},
    {NULL, 0, NULL, 0},
  };

  optind = 1; /* reset getopt(_long) state */
  while ((ch = getopt_long(mod->argc, mod->argv, optstr, opts, NULL)) != -1) {
    switch (ch) {
    case 'o':
      out_components_path = optarg;
      break;
    case 'c':
      out_compsvclist_path = optarg;
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

  if (!buf_init(&m->rowbuf, 512)) {
    goto fail_free_m;
  }

  m->reset = reset_new();
  if (m->reset == NULL) {
    fprintf(stderr, "failed to create reset for matcher\n");
    goto fail_buf_cleanup;
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

  if (out_components_path) {
    ret = opener_fopen(mod->opener, out_components_path, "wb",
        &m->out_components);
    if (ret < 0) {
      fprintf(stderr, "%s: %s\n", out_components_path,
          opener_strerror(mod->opener));
      goto fail_reset_free;
    }
    fputs("Component ID,Name,Version\r\n", m->out_components);
  }

  if (out_compsvclist_path) {
    ret = opener_fopen(mod->opener, out_compsvclist_path, "wb",
        &m->out_compsvclist);
    if (ret < 0) {
      fprintf(stderr, "%s: %s\n", out_compsvclist_path,
          opener_strerror(mod->opener));
      goto fail_fclose_components;
    }
    fputs("Component ID,Service ID\r\n", m->out_compsvclist);
  }

  ret = component_init(&m->ctbl);
  if (ret != 0) {
    goto fail_fclose_compsvclist;
  }

  mod->mod_data = m;
  return 0;
fail_fclose_compsvclist:
  if (m->out_compsvclist) {
    fclose(m->out_compsvclist);
  }
fail_fclose_components:
  if (m->out_components) {
    fclose(m->out_components);
  }
fail_reset_free:
  reset_free(m->reset);
fail_buf_cleanup:
  buf_cleanup(&m->rowbuf);
fail_free_m:
  free(m);
fail:
  return -1;
usage:
  fprintf(stderr,
      "usage: matcher [--out-components <path>]"
      " [--out-compsvclist <path>]\n");
  return -1;
}

void matcher_process(struct fetch_transfer *t, void *data) {
  struct matcher_data *m = data;
  int ret;
  int id;
  const char *header;
  size_t headerlen;
  enum reset_match_type type;

  /* see if any HTTP header patterns match, and if they do register the
   * match */
  headerlen = fetch_transfer_headerlen(t);
  if (headerlen > 0) {
    header = fetch_transfer_header(t);
    ret = reset_match(m->reset, header, headerlen);
    if (ret == RESET_ERR) {
      goto after_header;
    }

    while ((id = reset_get_next_match(m->reset)) >= 0) {
      type = reset_get_type(m->reset, id);
      if (type == RESET_MATCH_COMPONENT) {
        component_register(&m->ctbl,
            reset_get_name(m->reset, id),
            reset_get_substring(m->reset, id, header, headerlen, NULL),
            fetch_transfer_service_id(t));
      }
    }
  }
after_header:
  return;
}

static int svccmp(const void *a, const void *b) {
  return *(int*)a - *(int*)b;
}

static int print_component(void *data, void *value) {
  struct matcher_data *m = data;
  struct c_entry *c = value;
  int i;
  int ret;
  const char *compfields[3];
  const char *compsvcfields[2];
  char compidstr[24];
  char svcidstr[24];

  /* Encode the component as CSV and write it */
  if (m->out_components) {
    snprintf(compidstr, sizeof(compidstr), "%d", c->id);
    buf_clear(&m->rowbuf);
    compfields[0] = compidstr;
    compfields[1] = c->name;
    compfields[2] = c->version;
    ret = csv_encode(&m->rowbuf, compfields, ARRAY_SIZE(compfields));
    if (ret < 0) {
      return 1; /* skip it */
    }
    fwrite(m->rowbuf.data, 1, m->rowbuf.len, m->out_components);
  }

  /* Encode and write the component to services list as CSV */
  if (m->out_compsvclist) {
    qsort(c->services, c->slen, sizeof(c->services[0]), svccmp);
    for (i = 0; i < c->slen; i++) {
      buf_clear(&m->rowbuf);
      snprintf(svcidstr, sizeof(svcidstr), "%d", c->services[i]);
      compsvcfields[0] = compidstr;
      compsvcfields[1] = svcidstr;
      ret = csv_encode(&m->rowbuf, compsvcfields, ARRAY_SIZE(compsvcfields));
      if (ret < 0) {
        continue; /* skip it */
      }

      fwrite(m->rowbuf.data, 1, m->rowbuf.len, m->out_compsvclist);
    }
  }

  return 1;
}

void matcher_cleanup(void *data) {
  struct matcher_data *m = data;
  if (m) {
    /* finally - the time has come to dump our data! */
    component_foreach(&m->ctbl, print_component, m);

    component_cleanup(&m->ctbl);
    if (m->out_compsvclist) {
      fclose(m->out_compsvclist);
    }

    if (m->out_components) {
      fclose(m->out_components);
    }

    reset_free(m->reset);
    buf_cleanup(&m->rowbuf);
    free(m);
  }
}
