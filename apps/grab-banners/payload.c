#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netdb.h>

#include <apps/grab-banners/payload.h>

#define INITIAL_PAYLOAD_BUFSZ 256

static struct payload_data *payload_alloc() {
  struct payload_data *payload;

  payload = malloc(sizeof(*payload));
  if (payload == NULL) {
    goto fail;
  }

  if (!buf_init(&payload->buf, INITIAL_PAYLOAD_BUFSZ)) {
    goto free_payload;
  }

  return payload;
free_payload:
  free(payload);
fail:
  return NULL;
}

static int append_host_value(buf_t *buf, struct payload_host *h) {
  char hoststr[128];
  char portstr[8];
  int ret;

  ret = getnameinfo(h->sa, h->salen, hoststr, sizeof(hoststr), portstr,
      sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0) {
    return -1;
  }

  if (h->name == NULL || *h->name == '\0') {
    if (h->sa->sa_family != AF_INET) {
      buf_achar(buf, '[');
    }

    buf_adata(buf, hoststr, strlen(hoststr));

    if (h->sa->sa_family != AF_INET) {
      buf_achar(buf, ']');
    }
  } else {
    buf_adata(buf, h->name, strlen(h->name));
  }

  if (strcmp(portstr, "80") != 0 && strcmp(portstr, "443") != 0) {
    buf_achar(buf, ':');
    buf_adata(buf, portstr, strlen(portstr));
  }

  return 0;
}

struct payload_data *payload_build(const char *fmt, ...) {
  struct payload_data *payload;
  va_list ap;
  int ch;
  int ret;

  if (fmt == NULL) {
    return NULL;
  }

  payload = payload_alloc();
  if (payload == NULL) {
    goto fail;
  }

  va_start(ap, fmt);
  while ((ch = *fmt++) != '\0') {
    if (ch != '$') { /* fastpath: verbatim copy */
      buf_achar(&payload->buf, ch);
      continue;
    }

    switch((ch = *fmt++)) { /* $<X> variable substitution */
    case '$': /* Escaped $ sign */
      buf_achar(&payload->buf, '$');
      break;
    case 'H': {/* HTTP Host header value */
      struct payload_host *h = va_arg(ap, struct payload_host *);
      ret = append_host_value(&payload->buf, h);
      if (ret < 0) {
        goto fail_payload_free;
      }
      break;
    }
    default:
      buf_achar(&payload->buf, '$');
      buf_achar(&payload->buf, ch);
      break;
    }
  }

  va_end(ap);
  /* NB: payload->buf is not \0-terminated */
  return payload;

fail_payload_free:
  payload_free(payload);
fail:
  return NULL;
}

void payload_free(struct payload_data *payload) {
  if (payload) {
    buf_cleanup(&payload->buf);
    free(payload);
  }
}
