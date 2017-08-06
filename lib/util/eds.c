#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

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
#define EDS_CLIENT_REMOVED     (1 << 1)
#define EDS_CLIENT_RSUSPEND    (1 << 2)
#define EDS_CLIENT_IN_USE      (1 << 3)

/* in a listener process, this field gets the currently running service.
 * Used for graceful shutdown on HUP, TERM, INT signals */
static struct eds_service *running_service;

/* set to 1 when a listener receives SIGCHLD, cleared when reaping zombies */
static volatile sig_atomic_t got_listener_sigchld;

/* when running with a process supervisor, this variable is set in the
 * supervisor to initiate the shutdown of the children. */
static volatile sig_atomic_t shutdown_supervisor;

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

void eds_client_set_on_readable(struct eds_client *cli,
    void (*on_readable)(struct eds_client *cli, int fd)) {
  if (!(cli->flags & EDS_CLIENT_REMOVED)) {
    cli->actions.on_readable = on_readable;
    if (on_readable == NULL) {
      FD_CLR(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).rfds);
    } else {
      cli->flags &= ~EDS_CLIENT_RSUSPEND;
      FD_SET(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).rfds);
    }
  }
}

/* removes the current on_readable event and sets the EDS_CLIENT_RSUSPEND
 * flag. The file descriptor will not be checked in the event loop, but
 * the client will not be removed either. This is useful when we only want
 * to read from an fd after an external event has happened (e.g., read on
 * another fd, &c) */
void eds_client_suspend_readable(struct eds_client *cli) {
  if (cli->flags & EDS_CLIENT_REMOVED) {
    /* no-op if client is removed */
    return;
  }
  cli->actions.on_readable = NULL;
  cli->flags |= EDS_CLIENT_RSUSPEND;
}

void eds_client_clear_actions(struct eds_client *cli) {
  int fd = eds_client_get_fd(cli);
  cli->actions.on_readable = NULL;
  cli->actions.on_writable = NULL;
  cli->flags &= ~EDS_CLIENT_RSUSPEND;
  FD_CLR(fd, &EDS_SERVICE_LISTENER(cli->svc).rfds);
  FD_CLR(fd, &EDS_SERVICE_LISTENER(cli->svc).wfds);
}


void eds_client_set_on_writable(struct eds_client *cli,
    void (*on_writable)(struct eds_client *cli, int fd)) {
  if (!(cli->flags & EDS_CLIENT_REMOVED)) {
    cli->actions.on_writable = on_writable;
    if (on_writable == NULL) {
      FD_CLR(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).wfds);
    } else {
      FD_SET(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).wfds);
    }
  }
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
  /* XXX: We may be called from the event loop, or from within a
   *      client action at this point. If we are called from a client
   *      action, it's important that we don't make any assumptions about
   *      any actions registered after the call to eds_client_send */

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
  if (next != NULL) {
    cli->trans = *next;
  } else {
    cli->trans.flags = EDS_TFLREAD | EDS_TFLWRITE;
    cli->trans.on_readable = NULL;
    cli->trans.on_writable = NULL;
  }
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
  ecli->flags |= EDS_CLIENT_IN_USE;
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

int eds_client_set_ticker(struct eds_client *cli,
    void (*ticker)(struct eds_client *, int)) {
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(cli->svc);

  if (cli->svc->tick_slice_us == 0) {
    EDS_SERVE_ERR(cli->svc, "%s: fd %d: ticker set with zero tick_slice_us",
        cli->svc->name, eds_client_get_fd(cli));
    return -1;
  }

  if (cli->ticker == NULL && ticker != NULL) {
    /* adding a new ticker */
    l->ntickers++;
  } else if (cli->ticker != NULL && ticker == NULL) {
    /* clearing an existing ticker */
    l->ntickers--;
  }

  cli->ticker = ticker;
  return 0;
}

void eds_service_stop(struct eds_service *svc) {
  svc->flags |= EDS_SERVICE_STOPPED;
}

/* returns the delta, in microseconds, between to timespecs */
static long timespec_delta_us(struct timespec *start, struct timespec *end) {
  struct timespec tmp;

  if (end->tv_nsec < start->tv_nsec) {
    tmp.tv_sec = end->tv_sec - start->tv_sec - 1;
    tmp.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
  } else {
    tmp.tv_sec = end->tv_sec - start->tv_sec;
    tmp.tv_nsec = end->tv_nsec - start->tv_nsec;
  }

  return tmp.tv_sec*1000000 + tmp.tv_nsec/1000;
}

static void run_tickers(struct eds_service *svc,
    struct eds_service_listener *l) {
  int ntickers;
  int i;
  struct eds_client *cli;

  for (ntickers = l->ntickers, i = 0; ntickers > 0 && i < svc->nfds; i++) {
    cli = eds_service_client_from_fd(svc, i);
    if (cli->ticker != NULL) {
      cli->ticker(cli, i);
      ntickers--;
    }
  }
}

static void handle_listener_shutdown(int sig) {
  if (running_service) {
    running_service->flags |= EDS_SERVICE_STOPPED;
  }
}

static void handle_listener_sigchld(int sig) {
  got_listener_sigchld = 1;
}

static void reap_listener_children(struct eds_service *svc) {
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(svc);
  struct eds_client *cli;
  pid_t pid;
  int status;
  int i;

again:
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (svc->on_reaped_child) {
      for (i = 0; i < l->maxfd; i++) {
        cli = eds_service_client_from_fd(svc, i);
        if (cli->flags & EDS_CLIENT_IN_USE) {
          svc->on_reaped_child(svc, cli, pid, status);
        }
      }
    }
  }

  if (pid < 0 && errno == EINTR) {
    goto again;
  }

  got_listener_sigchld = 0;
}

static int eds_serve_single_mainloop(struct eds_service *svc) {
  struct sigaction sa = {{0}};
  struct eds_service_listener *l = &EDS_SERVICE_LISTENER(svc);
  struct eds_client *ecli;
  fd_set rfds;
  fd_set wfds;
  int num_fds;
  int i;
  int j;
  int fd;
  struct timeval tv;
  struct timeval *tvp;
  struct timespec last;
  struct timespec now;
  long expired_us;
  long left_us;

  signal(SIGPIPE, SIG_IGN);
  running_service = svc;
  sa.sa_handler = handle_listener_shutdown;
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sa.sa_handler = handle_listener_sigchld;
  sa.sa_flags = SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, NULL);


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

  /* call mod_init, if any. Should be done after udata allocation */
  if (svc->mod_init != NULL) {
    if (svc->mod_init(svc) < 0) {
      EDS_SERVE_ERR(svc, "%s: mod_init failure", svc->name);
      return -1;
    }
  }

  /* add the listening socket to the rfds set */
  FD_SET(svc->cmdfd, &l->rfds);
  l->maxfd = svc->cmdfd;

  /* select timeout is used for tickers. Initially, no tickers exists, so
   * we initialize the select timeout to NULL (meaning no timeout) */
  tvp = NULL;
  clock_gettime(CLOCK_MONOTONIC, &last);

select:
  if (svc->flags & EDS_SERVICE_STOPPED) {
    return 0;
  }

  if (got_listener_sigchld) {
    reap_listener_children(svc);
  }

  rfds = l->rfds;
  wfds = l->wfds;
  num_fds = select(l->maxfd + 1, &rfds, &wfds, NULL, tvp);
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

      /* this client may have been removed by another client whose
       * actions are run previously in the same select invocation. Do not
       * run any actions for this client if that's so */
      if (ecli->flags & EDS_CLIENT_REMOVED) {
        continue;
      }

      /* call action handlers */
      if (FD_ISSET(fd, &rfds) && ecli->actions.on_readable != NULL) {
        ecli->actions.on_readable(ecli, fd);
      }
      if (FD_ISSET(fd, &wfds) && ecli->actions.on_writable != NULL) {
        ecli->actions.on_writable(ecli, fd);
      }

      /* no action callbacks after called callbacks indicates termination,
       * unless reading is suspended */
      if (!(ecli->flags & EDS_CLIENT_RSUSPEND) &&
          ecli->actions.on_readable == NULL &&
          ecli->actions.on_writable == NULL) {
        eds_service_remove_client(svc, ecli);
      }
    }
  }

  if (l->ntickers > 0) {
    clock_gettime(CLOCK_MONOTONIC, &now);
    expired_us = timespec_delta_us(&last, &now);
    if (expired_us >= svc->tick_slice_us) {
      run_tickers(svc, l);
      last = now;
      tv.tv_sec = svc->tick_slice_us / 1000000;
      tv.tv_usec = svc->tick_slice_us % 1000000;
    } else {
      left_us = svc->tick_slice_us - expired_us;
      tv.tv_sec = left_us / 1000000;
      tv.tv_usec = left_us % 1000000;
    }
    tvp = &tv;
  } else {
    tvp = NULL; /* no tickers, set select timeout to NULL */
  }

  goto select;
}

void eds_service_remove_client(struct eds_service *svc,
    struct eds_client *cli) {
  struct eds_service_listener *l;
  int fd;

  /* first check if the client is removed, and return if it is. Immediately
   * after, mark it as being removed, to avoid recursion loops via on_done */
  if (cli->flags & EDS_CLIENT_REMOVED) {
    return;
  }
  cli->flags |= EDS_CLIENT_REMOVED;

  cli->flags &= ~EDS_CLIENT_IN_USE;

  l = &EDS_SERVICE_LISTENER(svc);
  fd = eds_client_get_fd(cli);

  if (cli->actions.on_done != NULL) {
    cli->actions.on_done(cli, fd);
    cli->actions.on_done = NULL;
  }

  if (cli->ticker != NULL) {
    l->ntickers--;
    cli->ticker = NULL;
  }

  if (!(cli->flags & EDS_CLIENT_FEXTERNALFD)) {
    close(fd);
    eds_client_set_externalfd(cli);
  }
  FD_CLR(fd, &l->rfds);
  FD_CLR(fd, &l->wfds);
  FD_SET(svc->cmdfd, &l->rfds);
}

/* initializes an eds_service struct. Called pre-fork on eds_serve */
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

  /* call mod_fini, if any */
  if (svc->mod_fini != NULL) {
    svc->mod_fini(svc);
  }

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

int eds_serve_single_by_name(struct eds_service *svcs, const char *name) {
  struct eds_service *svc;

  for (svc = svcs; svc->name != NULL; svc++) {
    if (strcmp(svc->name, name) == 0) {
      return eds_serve_single(svc);
    }
  }

  return -1;
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

static void handle_supervisor_shutdown(int sig) {
  if (sig == SIGINT || sig == SIGTERM || sig == SIGHUP) {
    shutdown_supervisor = 1;
  }
}

static int supervise_service_listeners(struct eds_service *svcs) {
  struct eds_service *svc;
  pid_t pid;
  int status;
  unsigned int i;

wait:
  pid = wait(&status);
  if (shutdown_supervisor) {
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

  sa.sa_handler = handle_supervisor_shutdown;
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

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
