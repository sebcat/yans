#include <lib/net/edd.h>
#include <assert.h>

#define EDD_SERVE_ERR(svc, fmt, ...) \
    if ((svc)->on_svc_error != NULL) { \
      (svc)->on_svc_error((svc), fmt, __VA_ARGS__); \
    }

typedef enum {
  CLEANUP_DONE,
  CLEANUP_ERROR,
} cleanup_status;

/* clean up the edd_client (base) fields, user allocated data is handled by
 * the service implementation. The service implementation must also free the
 * memory */
static void edd_client_cleanup(struct edd_client *cli, cleanup_status s) {
  size_t i;
  struct edd_service *svc = cli->svc;

  for (i = 0; i < EDD_MAXPASSEDFDS; i++) {
    if (cli->fds[i] >= 0) {
      close(cli->fds[i]);
    }
  }

  if (cli->toevent != NULL) {
    event_del(cli->toevent);
    event_free(cli->toevent);
  }

  if (cli->revent != NULL) {
    event_del(cli->revent);
    event_free(cli->revent);
  }

  io_close(&cli->io);
  if (s == CLEANUP_DONE) {
    svc->on_cli_done(cli);
  } else {
    svc->on_cli_error(cli);
  }
}

static void do_cliread(int fd, short ev, void *arg) {
  struct edd_client *cli = arg;
  edd_status s;

start:
  if (cli->state == EDDCLI_HASFDS) {
    /* we're past the FD receive state */
    s = cli->svc->on_cli_readable(cli);
    if (s == EDD_CONTINUE) {
      return;
    } else if (s == EDD_DONE) {
      cli->svc->on_cli_done(cli);
    } else {
      cli->svc->on_cli_error(cli);
    }
  } else {
    /* we're starting to, or continuing reading file descriptors */
    cli->state = EDDCLI_HASREAD;

    /* have we received all (if any) file descriptors? */
    if (cli->fdix >= cli->svc->nfds) {
      cli->state = EDDCLI_HASFDS;
      goto start;
    }

    if (io_recvfd(&cli->io, &cli->fds[cli->fdix++]) != IO_OK) {
      cli->svc->on_cli_error(cli);
      return;
    }
  }
}

static void do_readto(int fd, short ev, void *arg) {
  struct edd_client *cli = arg;
  edd_client_cleanup(cli, CLEANUP_ERROR);
}

static void do_svc_accept(int fd, short ev, void *arg) {
  struct edd_service *svc = arg;
  io_t client;
  unsigned int id;
  struct timeval tv;
  struct edd_client *cli = NULL;
  size_t i;

  /* accept the connection on the socket, or terminate the event loop
   * if the number of retries are exceeded */
  if (io_accept(&svc->io, &client) != IO_OK) {
    svc->aretries++;
    EDD_SERVE_ERR(svc, "%s: io_accept: %s", svc->name, io_strerror(&svc->io));
    if (svc->max_aretries > 0 && svc->aretries >= svc->max_aretries) {
      event_base_loopexit(svc->base, NULL);
    }
    return;
  }
  svc->aretries = 0; /* accept retries only applies to accept(2) */

  /* allocate the client */
  id = svc->id_counter++;
  cli = svc->on_accept(svc, &client, id);
  if (cli == NULL) {
    EDD_SERVE_ERR(svc, "%s: on_accept: failure (id:%u)", svc->name, id);
    goto fail;
  }

  /* init the client fields */
  assert(cli->magic == EDD_CLIENT_MAGIC);
  IO_INIT(&cli->io, IO_FILENO(&client));
  cli->id = id;
  cli->svc = svc;
  cli->fdix = 0;
  cli->toevent = NULL;
  cli->revent = NULL;
  cli->state = EDDCLI_INITED;
  for (i = 0; i < EDD_MAXPASSEDFDS; i++) {
    cli->fds[i] = -1;
  }

  /* setup the receive handler */
  cli->revent = event_new(svc->base, IO_FILENO(&client), EV_READ|EV_PERSIST,
      do_cliread, cli);
  if (cli->revent == NULL) {
    EDD_SERVE_ERR(svc, "%s: event_new: failure (id:%u)", svc->name, id);
    goto fail;
  }
  if (event_add(cli->revent, NULL) < 0) {
      EDD_SERVE_ERR(svc, "%s: event_add: failure (id:%u)", svc->name, id);
      goto fail;
  }

  /* setup the initial read timeout, if any */
  if (svc->to_secs > 0) {
    cli->toevent = evtimer_new(svc->base, do_readto, cli);
    if (cli->toevent == NULL) {
      EDD_SERVE_ERR(svc, "%s: evtimer_new: failure (id:%u)", svc->name, id);
      goto fail;
    }
    tv.tv_sec = svc->to_secs;
    tv.tv_usec = 0;
    if (evtimer_add(cli->toevent, &tv) < 0) {
      EDD_SERVE_ERR(svc, "%s: evtimer_add: failure (id:%u)", svc->name, id);
      goto fail;
    }
  }

  return;
fail:
  if (cli != NULL) {
    edd_client_cleanup(cli, CLEANUP_ERROR);
  }
}

int edd_serve(struct edd_service *svcs) {
  struct event_base *evbase = NULL;
  struct edd_service *svc;
  int status = -1;

  evbase = event_base_new();
  if (evbase == NULL) {
    return -1;
  }

  svc = svcs;
  while (svc->name != NULL) {
    assert(svc->path != NULL);
    assert(svc->on_accept != NULL);
    assert(svc->on_cli_done != NULL);
    assert(svc->on_cli_error != NULL);
    assert(svc->on_cli_readable != NULL || svc->nfds > 0);
    /* on_cli_writable is optional */
    assert(svc->nfds < EDD_MAXPASSEDFDS);

    if (io_listen_unix(&svc->io, svc->path) != IO_OK) {
      EDD_SERVE_ERR(svc, "%s: io_listen_unix: %s", svc->name,
          io_strerror(&svc->io));
      goto fail;
    }

    io_setnonblock(&svc->io, 1);
    svc->base = evbase;
    svc->lev = event_new(evbase, IO_FILENO(&svc->io), EV_READ|EV_PERSIST,
        do_svc_accept, svc);
    if (svc->lev == NULL) {
      EDD_SERVE_ERR(svc, "%s: event_new: failure", svc->name);
      io_close(&svc->io);
      goto fail;
    }

    event_add(svc->lev, NULL);
    svcs++;
  }

  status = event_base_dispatch(evbase);
  if (status == 1) {
    /* 1 means no events, which means we didn't pass any services */
    status = 0;
  }

fail:

  svc = svcs;
  while (svc->name != NULL) {
    if (svc->lev != NULL) {
      io_close(&svc->io);
      event_del(svc->lev);
      event_free(svc->lev);
    }
    svc++;
  }

  if (evbase != NULL) {
    event_base_free(evbase);
  }

  return status;
}
