#ifndef UTIL_EDS_H__
#define UTIL_EDS_H__

#include <sys/types.h>
#include <stddef.h>

#include <sys/select.h>

/* forward declarations */
struct eds_client;
struct eds_service;

struct eds_client_actions {
  void (*on_readable)(struct eds_client *cli, int fd);
  void (*on_writable)(struct eds_client *cli, int fd);
  void (*on_done)(struct eds_client *cli, int fd);
};

struct eds_client {
  int flags;
  struct eds_service *svc;
  struct eds_client_actions actions;
  char udata[]; /* service-specific user data, initialized to zero */
};

/* --- internal type representing the process supervisor --- */
struct eds_service_supervisor {
  pid_t *pids;
};

/* --- internal type representing a service process */
struct eds_service_listener {
  int maxfd;   /* highest file descriptor */
  fd_set rfds; /* readable file descriptor set */
  fd_set wfds; /* writable file descriptor set */
  void *cdata; /* array of struct eds_client w/ udata of udata_size */
};

struct eds_service {
  /* --- fields which may be set by the user of the module --- */
  char *name;         /* service name - used in log messages &c */
  char *path;         /* path to unix socket used for the service */
  size_t udata_size;  /* size of client specific data section */
  struct eds_client_actions actions; /* initial client actions */
  unsigned int nprocs; /* number of processes for handling clients */
  unsigned int nfds; /* number of fds in the client fd set */
  /* callback called on service error with string describing the error */
  void (*on_svc_error)(struct eds_service *svc, const char *err);


  /* --- fields that should be considered internal to the module --- */
  int cmdfd;
  int flags;
  union {
    struct eds_service_supervisor superv;
    struct eds_service_listener list;
  } iu;
};

#define EDS_SERVICE_SUPERVISOR(svc__) (svc__)->iu.superv
#define EDS_SERVICE_LISTENER(svc__) (svc__)->iu.list

int eds_client_get_fd(struct eds_client *cli);

/* mark the client fd as external, i.e., not closed when client is done */
void eds_client_set_externalfd(struct eds_client *cli);

static inline void eds_client_set_on_readable(struct eds_client *cli,
    void (*on_readable)(struct eds_client *cli, int fd)) {
  cli->actions.on_readable = on_readable;
  if (on_readable == NULL) {
    FD_CLR(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).rfds);
  } else {
    FD_SET(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).rfds);
  }
}

static inline void eds_client_set_on_writable(struct eds_client *cli,
    void (*on_writable)(struct eds_client *cli, int fd)) {
  cli->actions.on_writable = on_writable;
  if (on_writable == NULL) {
    FD_CLR(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).wfds);
  } else {
    FD_SET(eds_client_get_fd(cli), &EDS_SERVICE_LISTENER(cli->svc).wfds);
  }
}

static inline void eds_client_clear_actions(struct eds_client *cli) {
  int fd = eds_client_get_fd(cli);
  cli->actions.on_readable = NULL;
  cli->actions.on_writable = NULL;
  FD_CLR(fd, &EDS_SERVICE_LISTENER(cli->svc).rfds);
  FD_CLR(fd, &EDS_SERVICE_LISTENER(cli->svc).wfds);
}

struct eds_client *eds_service_add_client(struct eds_service *svc, int fd,
    struct eds_client_actions *acts, void *udata, size_t udata_size);

void eds_service_remove_client(struct eds_service *svc,
    struct eds_client *cli);

/* eds_service_stop --
 *   instructs a service to stop
 */
void eds_service_stop(struct eds_service *svc);

/* eds_serve_single --
 *   starts a service handler and serves incoming requests
 */
int eds_serve_single(struct eds_service *svc);

/* eds_serve --
 *   starts a set of service processes and serves incoming requests on them.
 *   Terminated service processes are always restarted by the supervisor
 *   process.
 *
 * side effects:
 *   signal handlers: SIGINT, SIGHUP, SIGTERM, SIGPIPE
 */
int eds_serve(struct eds_service *svcs);

#endif /* UTIL_EDS_H__ */
