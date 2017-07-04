#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>

#include <lib/util/io.h>
#include <lib/util/eds.h>

#define EDS_SERVE_ERR(svc__, fmt, ...) \
  if ((svc__)->on_svc_error != NULL) {   \
    svcerr((svc__), fmt, __VA_ARGS__); \
  }

/*internal  eds_service flags */
#define EDS_SERVICE_STOPPED (1 << 0)

/* internal eds_client flags */
#define EDS_CLIENT_FEXTERNALFD (1 << 0)

static void svcerr(struct eds_service *svc, const char *fmt, ...) {
  char errbuf[256];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
  svc->on_svc_error(svc, errbuf);
  va_end(ap);
}

static inline struct eds_client *eds_service_client_from_fd(
    struct eds_service *svc, int fd) {
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(svc);

  return (struct eds_client*)(((char*)l->cdata) +
      (sizeof(struct eds_client) + svc->udata_size) * fd);
}

int eds_client_get_fd(struct eds_client *cli) {
  char *base = (char*)EDS_SERVICE_LISTENER(cli->svc).cdata;
  char *off = (char*)cli;
  return (off - base) / (sizeof(struct eds_client) + cli->svc->udata_size);
}

void eds_client_set_externalfd(struct eds_client *cli) {
  cli->flags |= EDS_CLIENT_FEXTERNALFD;
}

static void on_eds_client_send(struct eds_client *cli, int fd) {
  ssize_t ret;

  if (cli->wrdatalen == 0) {
    goto completed;
  }

  do {
    ret = write(fd, cli->wrdata, cli->wrdatalen);
  } while (ret < 0 && errno == EINTR);

  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS) {
      return;
    }

    EDS_SERVE_ERR(cli->svc, "%s: fd %d write error: %s", cli->svc->name, fd,
        strerror(errno));
    eds_client_clear_actions(cli);
    return;
  }

  cli->wrdata += ret;
  cli->wrdatalen -= ret;
  if (cli->wrdatalen > 0) {
    return;
  }

completed:
  eds_client_clear_actions(cli);
  if (cli->trans.flags & EDS_TFLREAD) {
    eds_client_set_on_readable(cli, cli->trans.on_readable);
    cli->trans.flags &= ~EDS_TFLREAD;
  }

  if (cli->trans.flags & EDS_TFLWRITE) {
    eds_client_set_on_writable(cli, cli->trans.on_writable);
    cli->trans.flags &= ~EDS_TFLWRITE;
  }
}

void eds_client_send(struct eds_client *cli, const char *data, size_t len,
    struct eds_transition *next) {
  cli->wrdata = data;
  cli->wrdatalen = len;
  cli->trans = *next;
  eds_client_set_on_writable(cli, on_eds_client_send);
  on_eds_client_send(cli, eds_client_get_fd(cli));
}

struct eds_client *eds_service_add_client(struct eds_service *svc, int fd,
    struct eds_client_actions *acts, void *udata, size_t udata_size) {
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(svc);
  struct eds_client *ecli;
  io_t io;

  if (udata_size > svc->udata_size) {
    return NULL;
  }

  IO_INIT(&io, fd);
  io_setnonblock(&io, 1);
  if (fd > l->maxfd) {
    /* XXX: currently, l->maxfd is never decreased */
    l->maxfd = fd;
  }

  ecli = eds_service_client_from_fd(svc, fd);
  memset(ecli, 0, sizeof(struct eds_client) + svc->udata_size);
  ecli->svc = svc;
  ecli->actions = *acts;
  if (ecli->actions.on_readable != NULL) {
    FD_SET(fd, &l->rfds);
  }
  if (ecli->actions.on_writable != NULL) {
    FD_SET(fd, &l->wfds);
  }

  if (udata != NULL) {
    memcpy(ecli->udata, udata, udata_size);
  }

  /* stop accepting connections on cmdfd if we have reached the set limit */
  if (fd >= svc->nfds - 1) {
    FD_CLR(svc->cmdfd, &l->rfds);
  }

  return ecli;
}

void eds_service_stop(struct eds_service *svc) {
  svc->flags |= EDS_SERVICE_STOPPED;
}

static int eds_serve_single_mainloop(struct eds_service *svc) {
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(svc);
  struct eds_client *ecli;
  fd_set rfds;
  fd_set wfds;
  int num_fds;
  int i;
  int j;
  int fd;

  /* init listener fields */
  FD_ZERO(&l->rfds);
  FD_ZERO(&l->wfds);
  l->cdata = calloc(svc->nfds, sizeof(struct eds_client) + svc->udata_size);
  if (l->cdata == NULL) {
    EDS_SERVE_ERR(svc, "%s: failed to allocate client data (%u bytes)",
        svc->name,
        (sizeof(struct eds_client) + svc->udata_size) * svc->nfds);
    return -1;
  }

  FD_SET(svc->cmdfd, &l->rfds);
  l->maxfd = svc->cmdfd;

select:
  if (svc->flags & EDS_SERVICE_STOPPED) {
    return 0;
  }

  rfds = l->rfds;
  wfds = l->wfds;
  num_fds = select(l->maxfd + 1, &rfds, &wfds, NULL, NULL);
  if (num_fds < 0) {
    if (errno == EINTR) {
      goto select;
    } else {
      EDS_SERVE_ERR(svc, "%s: select: %s", svc->name, strerror(errno));
      return -1;
    }
  }

  if (FD_ISSET(svc->cmdfd, &rfds)) {
    /* accept a new client connection */
    fd = accept(svc->cmdfd, NULL, NULL);
    if (fd < 0) {
      if (errno == EWOULDBLOCK ||
          errno == EAGAIN ||
          errno == ECONNABORTED ||
          errno == EINTR) {
        goto select;
      }
      EDS_SERVE_ERR(svc, "%s: accept: %s", svc->name, strerror(errno));
      return -1;
    }

    if (eds_service_add_client(svc, fd, &svc->actions, NULL, 0) == NULL) {
      EDS_SERVE_ERR(svc, "%s: failed to add new client", svc->name);
    }

    FD_CLR(svc->cmdfd, &rfds); /* So we don't treat it as a client below */
    num_fds--;
  }

  /* XXX
   * since we don't want to check individual bits with FD_ISSET in select,
   * we assume that fd_set can be thought of as an array of unsigned longs.
   * this is however implementation specific and not defined by the standard.
   * These asserts are here to make sure things crash if fd_set is not of the
   * assumed size
   **/
   _Static_assert(((sizeof(fd_set) & (sizeof(unsigned long)-1)) == 0),
       "fd_set size is not divisable by the size of an unsigned long");
   _Static_assert(sizeof(fd_set)*8 == FD_SETSIZE,
       "fd_set size does no correlate with FD_SETSIZE");

  /* iterate over all possible file descriptors, call callbacks and cleanup
   * if needed. First check an unsigned long at a time, then if any bit is
   * set within the unsigned long chunk, check the individual bits */
  for(i = 0;
      num_fds > 0 && i < FD_SETSIZE / (sizeof(unsigned long) * 8);
      i++) {
    if (((unsigned long *)&rfds)[i] == 0 &&
        ((unsigned long *)&wfds)[i] == 0) {
      /* no readable or writable fds within this unsigned long chunk */
      continue;
    }

    for (j = 0; num_fds > 0 && j < sizeof(unsigned long) * 8; j++) {
      fd = sizeof(unsigned long) * 8 * i + j;
      if (!FD_ISSET(fd, &rfds) && !FD_ISSET(fd, &wfds)) {
        /* no action on this fd */
        continue;
      }

      /* There is an action on this fd */
      num_fds--;
      ecli = eds_service_client_from_fd(svc, fd);
      if (FD_ISSET(fd, &rfds) && ecli->actions.on_readable != NULL) {
        ecli->actions.on_readable(ecli, fd);
      }
      if (FD_ISSET(fd, &wfds) && ecli->actions.on_writable != NULL) {
        ecli->actions.on_writable(ecli, fd);
      }

      /* no action callbacks after called callbacks indicates termination */
      if (ecli->actions.on_readable == NULL &&
          ecli->actions.on_writable == NULL) {
        eds_service_remove_client(svc, ecli);
      }
    }
  }
  goto select;
}

void eds_service_remove_client(struct eds_service *svc,
    struct eds_client *cli) {
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(svc);
  int fd = eds_client_get_fd(cli);

  if (cli->actions.on_done != NULL) {
    cli->actions.on_done(cli, fd);
  }

  if (!(cli->flags & EDS_CLIENT_FEXTERNALFD)) {
    close(fd);
  }
  FD_CLR(fd, &l->rfds);
  FD_CLR(fd, &l->wfds);
  FD_SET(svc->cmdfd, &l->rfds);
}

static int eds_service_init(struct eds_service *svc) {
  io_t io;

  assert(svc->path != NULL);
  assert(svc->nfds <= FD_SETSIZE);
  assert(svc->actions.on_readable != NULL ||
      svc->actions.on_writable != NULL);
  if (svc->nfds == 0) {
    svc->nfds = FD_SETSIZE;
  }
  if (svc->nprocs == 0) {
    svc->nprocs = 1;
  }

  if (io_listen_unix(&io, svc->path) != IO_OK) {
    EDS_SERVE_ERR(svc, "%s: io_listen_unix: %s", svc->name,
        io_strerror(&io));
    return -1;
  }
  io_setnonblock(&io, 1);
  svc->cmdfd = IO_FILENO(&io);
  return 0;
}

static void eds_service_cleanup(struct eds_service *svc) {
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(svc);
  struct eds_client *cli;
  int i;


  /* cleanup connected clients */
  for (i = 0; i < FD_SETSIZE; i++) {
    if ((FD_ISSET(i, &l->rfds) || FD_ISSET(i, &l->wfds)) && i != svc->cmdfd) {
      cli = eds_service_client_from_fd(svc, i);
      eds_service_remove_client(svc, cli);
    }
  }

  /* cleanup command file descriptor */
  FD_CLR(svc->cmdfd, &l->rfds);
  close(svc->cmdfd);
  svc->cmdfd = -1;

  /* free client data */
  free(l->cdata);
  l->cdata = NULL;
}

int eds_serve_single(struct eds_service *svc) {
  int ret;

  if (eds_service_init(svc) < 0) {
    return -1;
  }

  ret = eds_serve_single_mainloop(svc);
  eds_service_cleanup(svc);
  return ret;
}

static int fork_service_listener(struct eds_service *svc, unsigned int n) {
  pid_t pid;
  int ret;

  pid = fork();
  if (pid < 0) {
    return -1;
  } else if (pid > 0) {
    EDS_SERVICE_SUPERVISOR(svc).pids[n] = pid;
    return 0;
  }

  /* in child; reset signal handlers, free mem and start child handler */
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  free(EDS_SERVICE_SUPERVISOR(svc).pids);
  EDS_SERVICE_SUPERVISOR(svc).pids = NULL;
  ret = eds_serve_single_mainloop(svc);
  eds_service_cleanup(svc);
  if (ret < 0) {
    exit(1);
  } else {
    exit(0);
  }
}

static void stop_service_listeners(struct eds_service *svcs) {
  struct eds_service *svc;
  unsigned int i;

  for (svc = svcs; svc->name != NULL; svc++) {
    if (EDS_SERVICE_SUPERVISOR(svc).pids == NULL) {
      continue;
    }

    for (i = 0; i < svc->nprocs; i++) {
      if (EDS_SERVICE_SUPERVISOR(svc).pids[i] != -1) {
        kill(EDS_SERVICE_SUPERVISOR(svc).pids[i], SIGTERM);
      }
    }
  }

  sleep(1);

  for (svc = svcs; svc->name != NULL; svc++) {
    if (EDS_SERVICE_SUPERVISOR(svc).pids == NULL) {
      continue;
    }

    for (i = 0; i < svc->nprocs; i++) {
      kill(EDS_SERVICE_SUPERVISOR(svc).pids[i], SIGKILL);
      EDS_SERVICE_SUPERVISOR(svc).pids[i] = -1;
    }

    free(EDS_SERVICE_SUPERVISOR(svc).pids);
    EDS_SERVICE_SUPERVISOR(svc).pids = NULL;
  }
}

static int start_service_listeners(struct eds_service *svc) {
  unsigned int i;

  assert(svc != NULL);
  assert(EDS_SERVICE_SUPERVISOR(svc).pids == NULL);

  EDS_SERVICE_SUPERVISOR(svc).pids = malloc(svc->nprocs * sizeof(pid_t));
  if (EDS_SERVICE_SUPERVISOR(svc).pids == NULL) {
    return -1;
  }

  for (i = 0; i < svc->nprocs; i++) {
    EDS_SERVICE_SUPERVISOR(svc).pids[i] = -1;
  }

  for (i = 0; i < svc->nprocs; i++) {
    if (fork_service_listener(svc, i) < 0) {
      goto fail;
    }
  }

  return 0;
fail:
  return -1;
}

/* set once, never reset */
static volatile sig_atomic_t got_shutdown_sig = 0;

static void handle_shutdown_sig(int sig) {
  if (sig == SIGINT || sig == SIGTERM || sig == SIGHUP) {
    got_shutdown_sig = 1;
  }
}

static int supervise_service_listeners(struct eds_service *svcs) {
  struct eds_service *svc;
  pid_t pid;
  int status;
  unsigned int i;

wait:
  pid = wait(&status);
  if (got_shutdown_sig) {
    return 0;
  } else if (pid < 0 && errno == EINTR) {
    goto wait;
  }

  if (pid < 0) {
    /* can't log anything here since it's not service-specific */
    return -1;
  }

  if (!WIFEXITED(status) && !WIFSIGNALED(status)) {
    goto wait;
  }

  /* find the corresponding service */
  svc = svcs;
  for (svc = svcs; svc->name != NULL; svc++) {
    for (i = 0; i < svc->nprocs; i++) {
      if (EDS_SERVICE_SUPERVISOR(svc).pids[i] == pid) {
        EDS_SERVICE_SUPERVISOR(svc).pids[i] = -1;
        if (WIFEXITED(status)) {
          EDS_SERVE_ERR(svc,
              "%s: child:%u PID:%d exited (status: %d)",
              svc->name, i, pid, WEXITSTATUS(status));
        } else {
          EDS_SERVE_ERR(svc,
              "%s: child:%u PID:%d terminated (signal: %d)",
              svc->name, i, pid, WTERMSIG(status));
        }
        if (fork_service_listener(svc, i) < 0) {
          EDS_SERVE_ERR(svc, "%s: restart failure (child:%u)",
              svc->name, i);
          /* XXX: how to handle? is the fault transient? persistent? */
          goto wait;
        }
        goto wait;
      }
    }
  }

  /*
    XXX: which error handler?
    EDS_SERVE_ERR(svc, "%s: got termination from unknown child PID:%d", pid);
  */
  goto wait;
}

int eds_serve(struct eds_service *svcs) {
  struct eds_service *svc = svcs;
  int status = -1;
  struct sigaction sa = {{0}};

  /* XXX
   * setting up signal handlers (or any global state) inside lib code should
   * be considered bad, but it's probably OK for eds_serve's use case */
  sa.sa_handler = handle_shutdown_sig;
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  for(svc = svcs; svc->name != NULL; svc++) {
    if (eds_service_init(svc) < 0) {
      goto done;
    }
    if (start_service_listeners(svc) < 0) {
      EDS_SERVE_ERR(svc, "%s: start_service_listeners: %s", svc->name,
          strerror(errno));
      goto done;
    }
  }

  status = supervise_service_listeners(svcs);

done:
  stop_service_listeners(svcs);
  return status;
}