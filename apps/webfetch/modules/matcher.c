/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <lib/util/macros.h>
#include <lib/util/csv.h>
#include <lib/match/reset.h>
#include <lib/match/component.h>
#include <apps/webfetch/modules/matcher.h>

#define DEFAULT_OUTPATH "-"

/* genmatcher output */
#include <apps/webfetch/modules/matcher_httpheader.c>
#include <apps/webfetch/modules/matcher_httpbody.c>

struct matcher_data {
  reset_t *httpheader_reset;
  reset_t *httpbody_reset;
  struct component_ctx ctbl;
  FILE *out_compsvc_csv;
  buf_t rowbuf;
};

static int init_matcher_reset(reset_t **out_reset,
    const struct reset_pattern *patterns, size_t npatterns) {
  reset_t *reset;
  int ret;

  reset = reset_new();
  if (reset == NULL) {
    fprintf(stderr, "failed to create reset for matcher\n");
    return -1;
  }

  ret = reset_load(reset, patterns, npatterns);
  if (ret != RESET_OK) {
    fprintf(stderr, "reset_load: %s\n", reset_strerror(reset));
    reset_free(reset);
    return -1;
  }

  *out_reset = reset;
  return 0;
}

int matcher_init(struct module_data *mod) {
  struct matcher_data *m;
  int ret;
  int ch;
  const char *out_compsvc_path = NULL;
  char *optstr = "o:";
  struct option opts[] = {
    {"out-compsvc-csv", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0},
  };

  optind = 1; /* reset getopt(_long) state */
  while ((ch = getopt_long(mod->argc, mod->argv, optstr, opts, NULL)) != -1) {
    switch (ch) {
    case 'o':
      out_compsvc_path = optarg;
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

  ret = init_matcher_reset(&m->httpheader_reset, httpheader_,
      ARRAY_SIZE(httpheader_));
  if (ret != 0) {
    fprintf(stderr, "failed to init httpheader reset\n");
    goto fail_buf_cleanup;
  }

  ret = init_matcher_reset(&m->httpbody_reset, httpbody_,
      ARRAY_SIZE(httpbody_));
  if (ret != 0) {
    fprintf(stderr, "failed to init httpbody reset\n");
    goto fail_httpheader_reset_free;
  }

  if (out_compsvc_path) {
    ret = opener_fopen(mod->opener, out_compsvc_path, "wb",
        &m->out_compsvc_csv);
    if (ret < 0) {
      fprintf(stderr, "%s: %s\n", out_compsvc_path,
          opener_strerror(mod->opener));
      goto fail_httpbody_reset_free;
    }
    fputs("Name,Version,Service ID\r\n", m->out_compsvc_csv);
  }

  ret = component_init(&m->ctbl);
  if (ret != 0) {
    goto fail_fclose_compsvc;
  }

  mod->mod_data = m;
  return 0;
fail_fclose_compsvc:
  if (m->out_compsvc_csv) {
    fclose(m->out_compsvc_csv);
  }
fail_httpbody_reset_free:
  reset_free(m->httpbody_reset);
fail_httpheader_reset_free:
  reset_free(m->httpheader_reset);
fail_buf_cleanup:
  buf_cleanup(&m->rowbuf);
fail_free_m:
  free(m);
fail:
  return -1;
usage:
  fprintf(stderr,
      "usage: matcher [--out-compsvc-csv <path>]"
      " [--out-compsvclist <path>]\n");
  return -1;
}

static void process_matches(struct component_ctx *ctbl, reset_t *reset,
    const void *data, size_t len, long service_id) {
  int ret;
  int id;
  enum reset_match_type type;

  ret = reset_match(reset, data, len);
  if (ret == RESET_ERR) {
    return;
  }

  while ((id = reset_get_next_match(reset)) >= 0) {
    type = reset_get_type(reset, id);
    if (type == RESET_MATCH_COMPONENT) {
      component_register(ctbl,
          reset_get_name(reset, id),
          reset_get_substring(reset, id, data, len, NULL),
          service_id);
    }
  }
}

void matcher_process(struct fetch_transfer *t, void *matcherdata) {
  struct matcher_data *m = matcherdata;
  const char *data;
  size_t len;

  /* see if any HTTP header patterns match, and if they do register the
   * match */
  len = fetch_transfer_headerlen(t);
  if (len > 0) {
    data = fetch_transfer_header(t);
    process_matches(&m->ctbl, m->httpheader_reset, data, len,
        fetch_transfer_service_id(t));
  }

  /* see if any HTTP body patterns match, and if they do register the
   * match */
  len = fetch_transfer_bodylen(t);
  if (len > 0) {
    data = fetch_transfer_body(t);
    process_matches(&m->ctbl, m->httpbody_reset, data, len,
        fetch_transfer_service_id(t));
  }
}

static int print_component(void *data, void *value) {
  struct matcher_data *m = data;
  struct component_entry *c = value;
  int ret;
  const char *compfields[3];
  char svcidstr[24];
  size_t svc;

  /* Encode the component as CSV and write it */
  if (m->out_compsvc_csv) {
    for (svc = 0; svc < c->slen; svc++) {
      snprintf(svcidstr, sizeof(svcidstr), "%d", c->services[svc]);
      buf_clear(&m->rowbuf);
      compfields[0] = c->name;
      compfields[1] = c->version;
      compfields[2] = svcidstr;
      ret = csv_encode(&m->rowbuf, compfields, ARRAY_SIZE(compfields));
      if (ret < 0) {
        return 1; /* skip it */
      }
      fwrite(m->rowbuf.data, 1, m->rowbuf.len, m->out_compsvc_csv);
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
    if (m->out_compsvc_csv) {
      fclose(m->out_compsvc_csv);
    }

    reset_free(m->httpbody_reset);
    reset_free(m->httpheader_reset);
    buf_cleanup(&m->rowbuf);
    free(m);
  }
}
