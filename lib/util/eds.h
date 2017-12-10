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
  void (*on_finalize)(struct eds_client *cli);
};

/* eds transition flags */
#define EDS_TFLREAD  (1 << 0)
#define EDS_TFLWRITE (1 << 1)

/* eds handler flags */
#define EDS_DEFER    (1 << 0)

/* for eds functions that needs a transition to another state when done
 * e.g., eds_client_send, the eds_transition struct holds the callbacks
 * that shall be used on a successful transition */
struct eds_transition {
  int flags;
  void (*on_readable)(struct eds_client *cli, int fd);
  void (*on_writable)(struct eds_client *cli, int fd);
};

struct eds_client {
  int flags;
  struct eds_service *svc;
  struct eds_client_actions actions;

  /* eds_client_send (and possibly others) state */
  struct eds_transition trans;
  const char *wrdata;
  size_t wrdatalen;

  /* ticker (if any) */
  void (*ticker)(struct eds_client *, int);

  char udata[]; /* service-specific user data, initialized to zero */
};

/* --- internal type representing the process supervisor --- */
struct eds_service_supervisor {
  pid_t *pids;
};

/* --- internal type representing a service process */
struct eds_service_listener {
  int ntickers; /* number of currently set tickers */
  int maxfd;    /* highest file descriptor */
  fd_set rfds;  /* readable file descriptor set */
  fd_set wfds;  /* writable file descriptor set */
  void *cdata;  /* array of struct eds_client w/ udata of udata_size */
};

struct eds_service {
  /* --- fields which may be set by the user of the module --- */
  const char *name;   /* service name - used in log messages &c */
  const char *path;   /* path to unix socket used for the service */
  size_t udata_size;  /* size of client specific data section */
  struct eds_client_actions actions; /* initial client actions */
  unsigned int nprocs; /* number of processes for handling clients */
  unsigned int nfds; /* number of fds in the client fd set */
  unsigned int tick_slice_us; /* tick time slice, in microseconds */
  /* callback called on service error with string describing the error */
  void (*on_svc_error)(struct eds_service *svc, const char *err);
  /* callback called on reaped child for every eds_client in use */
  void (*on_reaped_child)(struct eds_service *svc, struct eds_client *cli,
      pid_t pid, int status);

  /* init, fini routines, if any. Called once per process  */
  int (*mod_init)(struct eds_service *svc); /* ret < 0 means failure */
  void (*mod_fini)(struct eds_service *svc);

  /* data associated with the service */
  union {
    void *ptr;
    size_t s;
    int i;
  } svc_data;


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

/* set eds_client actions */
void eds_client_set_on_readable(struct eds_client *cli,
    void (*on_readable)(struct eds_client *cli, int fd), int flags);
void eds_client_set_on_writable(struct eds_client *cli,
    void (*on_writable)(struct eds_client *cli, int fd), int flags);
void eds_client_suspend_readable(struct eds_client *cli);
void eds_client_clear_actions(struct eds_client *cli);

/* send a piece of data, and transition to the next state on success. On write
 * failure, log an error and remove the client */
void eds_client_send(struct eds_client *cli, const char *data, size_t len,
    struct eds_transition *next);

struct eds_client *eds_service_add_client(struct eds_service *svc, int fd,
    struct eds_client_actions *acts);

int eds_client_set_ticker(struct eds_client *cli,
    void (*ticker)(struct eds_client *, int));

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

/* iterates over svcs until sentinel, starts the first service identified
 * by name */
int eds_serve_single_by_name(struct eds_service *svcs, const char *name);

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
