#ifndef NET_EDD_H
#define NET_EDD_H

#include <event2/event.h>
#include <lib/util/io.h>

#define EDD_SERVICE_ERR(svc, prefix, msg) \
    ylog_error("%s " prefix ": %s", (svc)->name, (msg));

/* maximum number of file descriptors that can be passed initially to a
 * service listener */
#define EDD_MAXPASSEDFDS 8

#define EDD_CLIENT_MAGIC 0x5F91F29A

struct edd_service;

/* service client event handler return status */
typedef enum {
  EDD_FAILURE = -1, /* clean up the client and call the error handler */
  EDD_DONE,         /* successful return, keep the event */
  EDD_CONTINUE,     /* successful return, unregister the event */
} edd_status;

/* service client state (initial) */
typedef enum {
  EDDCLI_INITED,
  EDDCLI_HASREAD,  /* client read callback called */
  EDDCLI_HASFDS,   /* client has received all fds (if any) */
} edd_client_state;


/* base struct for service clients, should be embedded in specific edd_client's
 * in need of state */
struct edd_client {
  int magic;                  /* number used to verify edd client structs */
  io_t io;                    /* client I/O handler */
  unsigned int id;            /* client ID */
  struct edd_service *svc;    /* service listener, not owned by edd_client */
  int fds[EDD_MAXPASSEDFDS];  /* file descriptors passed to the client */

  /* -- internal fields below this line -- */
  int fdix;                   /* file descriptor index */
  edd_client_state state;     /* which state the client has reached */
  struct event *toevent;      /* initial read timeout event */
  struct event *revent;       /* readable event */
};

/* data type representing an edd service listener.
 * the struct is expected to be zero-inited, e.g., by being statically
 * allocated */
struct edd_service {
  /* the name of the service - can be used in log messages &c */
  char *name;

  /* the path to the listener UNIX socket, which will be created/overwritten */
  char *path;

  /* maximum number of accept retries. zero == unlimited */
  unsigned int max_aretries;

  /* timeout, in seconds, before a client is connected to the first read event
   * 0 means no timeout */
  unsigned int to_secs;

  /* number of file descriptors expected to be passed by the client */
  unsigned int nfds;

  /* service error callback (optional) */
  void (*on_svc_error)(struct edd_service *svc, char *fmt, ...);

  /* initializes a new client on a newly accepted connection and returns a
   * pointer to it. If NULL is returned the connection is aborted */
  struct edd_client *(*on_accept)(struct edd_service *svc, io_t *cli,
      unsigned int id);

  /* client event callbacks */
  edd_status (*on_cli_readable)(struct edd_client *cli);
  edd_status (*on_cli_writable)(struct edd_client *cli);
  void (*on_cli_done)(struct edd_client *cli);
  void (*on_cli_error)(struct edd_client *cli);

  /* -- internal fields below this line -- */
  struct event_base *base;    /* event loop shared between service handlers */
  struct event *lev;          /* accept event for listener */
  io_t io;                    /* listener I/O handler */
  int aretries;               /* current number of accept retries */
  unsigned int id_counter;    /* ID counter for connected clients */
};

int edd_serve(struct edd_service *svcs);

#endif /* NET_EDD_H */
