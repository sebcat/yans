#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <lib/util/ylog.h>
#include <lib/util/zfile.h>
#include <lib/net/dnstres.h>
#include <apps/clid/resolver.h>

#define DFL_NRESOLVERS 10

#define RESOLVERFL_HASMSGBUF (1 << 0)

#define LOGERR(...) \
    ylog_error(__VA_ARGS__)

#define LOGINFO(...) \
    ylog_info(__VA_ARGS__)

#define RESOLVER_CLI(cli__) \
    (struct resolver_cli*)((cli__)->udata)

static unsigned short nresolvers_ = DFL_NRESOLVERS;
static struct dnstres_pool *trespool_;
static sigset_t sigset_;
static pthread_t sigthread_;

struct resolver_data {
  FILE *resfile;
  int closefd;
};

static void on_readfd(struct eds_client *cli, int fd);

void resolver_set_nresolvers(unsigned short nresolvers) {
  if (nresolvers > 0) {
    nresolvers_ = nresolvers;
  }
}

static void *sigthread(void *data) {
  int ret;
  int sig;

  ret = sigwait(&sigset_, &sig);
  if (ret != 0) {
    LOGERR("sigwait failure");
    exit(1);
  }

  if (sig == SIGHUP || sig == SIGINT || sig == SIGTERM) {
    /* TODO: we should probably have a thread safe ylog backend for this */
    LOGERR("caught signal %d, shutting down", sig);
    dnstres_pool_free(trespool_);
    exit(0);
  } else {
    LOGERR("unexpected signal %d", sig);
    exit(1);
  }
  return NULL;
}

int resolver_init(struct eds_service *svc) {
  int ret;
  struct dnstres_pool_opts opts = {
    .nthreads = nresolvers_,
    .stacksize = 0,
  };

  /* ah, signals and threads... Hello darkness my old friend... */
  /* we mask SIGHUP, SIGINT, SIGTERM here, and let a signal handler
   * thread deal with them synchronously. The signal handler thread will then
   * call exit after the dnstres thread pool is shut down gracefully */
  sigemptyset(&sigset_);
  sigaddset(&sigset_, SIGHUP);
  sigaddset(&sigset_, SIGINT);
  sigaddset(&sigset_, SIGTERM);
  ret = pthread_sigmask(SIG_BLOCK, &sigset_, NULL);
  if (ret != 0) {
    LOGERR("resolver: sigmask setup failure");
    return -1;
  }

  /* create the pool of resolver threads */
  trespool_ = dnstres_pool_new(&opts);
  if (trespool_ == NULL) {
    LOGERR("resolver: failed to start resolver pool");
    return -1;
  }

  /* create the signal handler thread. Must be done after trespool_ is
   * created */
  ret = pthread_create(&sigthread_, NULL, sigthread, NULL);
  if (ret != 0) {
    LOGERR("resolver: unable to create signal handler thread");
    dnstres_pool_free(trespool_);
    return -1;
  }

  return 0;
}

void res_on_resolved(void *data, const char *host, const char *addr) {
  struct resolver_data *resdata = data;

  /* we have resolved a host to an address, write it to the result file */
  flockfile(resdata->resfile);
  fprintf(resdata->resfile, "%s %s\n", host, addr);
  funlockfile(resdata->resfile);
}

void res_on_done(void *data) {
  struct resolver_data *resdata = data;

  /* we're done. Close the result file, the closefd and free the
   * resolver_data. The result file must be closed first to be sure that all
   * data is flushed. */
  fclose(resdata->resfile);
  close(resdata->closefd);
  free(resdata);
}

static void on_sendclosefd(struct eds_client *cli, int fd) {
  struct resolver_cli *ecli = RESOLVER_CLI(cli);
  struct resolver_data *resdata;
  struct dnstres_request req;
  int ret;

  /* send the fd used to signal when the resolving is done */
  ret = ycl_sendfd(&ecli->ycl, ecli->closefds[1], 0);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    LOGERR("resolvercli%d: send closefd: %s", fd, ycl_strerror(&ecli->ycl));
    goto fail;
  }

  /* close and clear client closefd */
  close(ecli->closefds[1]);
  ecli->closefds[1] = -1;


  /* allocate data structure for the resolver */
  resdata = malloc(sizeof(*resdata));
  if (resdata == NULL) {
    goto fail;
  }

  /* setup the resolver request */
  resdata->resfile = ecli->resfile;
  resdata->closefd = ecli->closefds[0];
  req.hosts = ecli->hosts;
  req.data = resdata;
  req.on_resolved = res_on_resolved;
  req.on_done = res_on_done;

  /* cleanup ecli fields so they're not free'd separately on closed fd.
   * remaining stuff will be cleaned up by the resolver when done */
  ecli->closefds[0] = -1;
  ecli->resfile = NULL;
  ecli->hosts = NULL;

  /* setup next states */
  eds_client_set_on_readable(cli, on_readfd, EDS_DEFER);
  eds_client_set_on_writable(cli, NULL, 0);


  /* start the resolver and return, waiting for the next request */
  dnstres_pool_add_hosts(trespool_, &req);
  return;

fail:
  eds_client_clear_actions(cli);
}

static void on_readreq(struct eds_client *cli, int fd) {
  struct resolver_cli *ecli = RESOLVER_CLI(cli);
  struct ycl_msg_resolver_req req = {{0}};
  int ret;

  /* receive the request, containing the hosts to resolve */
  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    LOGERR("resolvercli%d: readreq: %s", fd, ycl_strerror(&ecli->ycl));
    goto fail;
  }

  ret = ycl_msg_parse_resolver_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    LOGERR("resolvercli%d: resolver_req parse error", fd);
    goto fail;
  }

  /* create the socketpair that will be used to signal when the resolving
   * is done */
  ret = socketpair(AF_UNIX, SOCK_STREAM, 0, ecli->closefds);
  if (ret < 0) {
    LOGERR("resolvercli%d: socketpair: %s", fd, strerror(errno));
    goto fail;
  }

  ecli->hosts = req.hosts.data;
  eds_client_set_on_readable(cli, NULL, 0);
  eds_client_set_on_writable(cli, on_sendclosefd, 0);
  return;
fail:
  eds_client_clear_actions(cli);
}

static void on_readfd(struct eds_client *cli, int fd) {
  struct resolver_cli *ecli = RESOLVER_CLI(cli);
  int resfd = -1;
  int ret;

  /* receive the file descriptor that will receive the resolved hosts */
  ret = ycl_recvfd(&ecli->ycl, &resfd);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    goto fail;
  }

  /* create the FILE* for writing compressed data to fd */
  ecli->resfile = zfile_fdopen(resfd, "wb");
  if (ecli->resfile == NULL) {
    LOGERR("resolvercli%d: unable to open result file", fd);
    goto cleanup_resfd;
  }

  eds_client_set_on_readable(cli, on_readreq, 0);
  return;
cleanup_resfd:
  close(resfd);
fail:
  eds_client_clear_actions(cli);
}

void resolver_on_readable(struct eds_client *cli, int fd) {
  struct resolver_cli *ecli = RESOLVER_CLI(cli);
  int ret;

  ycl_init(&ecli->ycl, fd);
  if (ecli->flags & RESOLVERFL_HASMSGBUF) {
    ycl_msg_reset(&ecli->msgbuf);
  } else {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      LOGERR("resolvercli%d: ycl_msg_init failure", fd);
      goto fail;
    }
    ecli->flags |= RESOLVERFL_HASMSGBUF;
  }

  eds_client_set_on_readable(cli, on_readfd, 0);
  return;

fail:
  eds_client_clear_actions(cli);
}

void resolver_cli_cleanup(struct resolver_cli *ecli) {
  /* cleanup the resolver_cli fields that may or may not be set, depending
   * on in which part of the state machine the client was done */
  if (ecli->closefds[0] >= 0) {
    close(ecli->closefds[0]);
    ecli->closefds[0] = -1;
  }

  if (ecli->closefds[1] >= 0) {
    close(ecli->closefds[1]);
    ecli->closefds[1] = -1;
  }

  if (ecli->resfile != NULL) {
    fclose(ecli->resfile);
    ecli->resfile = NULL;
  }

  ecli->hosts = NULL; /* points into ecli->msgbuf, free handled separately */
}


void resolver_on_done(struct eds_client *cli, int fd) {
  struct resolver_cli *ecli = RESOLVER_CLI(cli);
  LOGINFO("resolvercli%d: done", fd);
  resolver_cli_cleanup(ecli);
  eds_client_clear_actions(cli);
}

void resolver_on_finalize(struct eds_client *cli) {
  struct resolver_cli *ecli = RESOLVER_CLI(cli);
  if (ecli->flags & RESOLVERFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~RESOLVERFL_HASMSGBUF;
  }
}
