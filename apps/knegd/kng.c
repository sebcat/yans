#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <lib/util/ylog.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/knegd/kng.h>

#ifndef DATAROOTDIR
#define DATAROOTDIR "/usr/local/share"
#endif

#ifndef YSCANSDIR
#define YSCANSDIR DATAROOTDIR "/yscans"
#endif

/* valid characters for scan types - must not contain path chars &c */
#define VALID_TYPECH(ch__) \
  (((ch__) >= 'a' && (ch__) <= 'z') || \
   ((ch__) >= '0' && (ch__) <= '9') || \
   ((ch__) == '-'))

#define KNGFL_HASMSGBUF (1 << 0)

#define LOGERR(...) \
    ylog_error(__VA_ARGS__)

#define LOGINFO(...) \
    ylog_info(__VA_ARGS__)

struct kng_ctx {
  struct kng_ctx *next;
  char *id;
  pid_t pid;
  int sock;
};

static int is_valid_type(const char *t) {
  char ch;

  if (t == NULL || *t == '\0') {
    return 0;
  }

  while ((ch = *t) != '\0') {
    if (!VALID_TYPECH(ch)) {
      return 0;
    }
    t++;
  }

  return 1;
}

static void free_envp(char **envp) {
  size_t i;

  if (envp) {
    for (i = 0; envp[i] != NULL; i++) {
      free(envp[i]);
    }
    free(envp);
  }
}

static char *consk(const char *name, const char *value) {
  size_t namelen;
  size_t valuelen;
  size_t totlen;
  char *str;

  assert(name != NULL);
  assert(value != NULL);
  namelen = strlen(name);
  valuelen = strlen(value);
  totlen = namelen + valuelen + 2;
  str = malloc(totlen);
  if (str == NULL) {
    return NULL;
  }
  snprintf(str, totlen, "%s=%s", name, value);
  return str;
}

static char *consp(const char *prefix, const char *param) {
  size_t prefixlen;
  size_t paramlen;
  size_t totlen;
  char *str;

  assert(prefix != NULL);
  assert(param != NULL);
  prefixlen = strlen(prefix);
  paramlen = strlen(param);
  totlen = prefixlen + paramlen + 2;
  str = malloc(totlen);
  if (str == NULL) {
    return NULL;
  }
  snprintf(str, totlen, "%s_%s", prefix, param);
  return str;
}

/* known, constant names */
#define ENVP_VAR(name__, value__) \
    if ((value__) != NULL && *(value__) != '\0') {   \
      envp[off] = consk((name__), (value__));        \
      if (envp[off] == NULL) {                       \
        goto fail;                                   \
      }                                              \
      off++;                                         \
    }

/* arbitrary vars prefixed with known, constant prefix */
#define ENVP_PARAM(prefix__, param__)                \
    if ((param__) != NULL && *(param__) != '\0') {   \
      envp[off] = consp((prefix__), (param__));      \
      if (envp[off] == NULL) {                       \
        goto fail;                                   \
      }                                              \
      off++;                                         \
    }


/* build the environment from the knegd request */
static char **mkenvp(struct ycl_msg_knegd_req *req, const char *id) {
  size_t off = 0;
  size_t i;
  char **envp;

  envp = calloc(req->nparams + 3, sizeof(char*));
  if (envp == NULL) {
    return NULL;
  }

  ENVP_VAR("KNEGD_ID", id);
  ENVP_VAR("KNEGD_TYPE", req->type);
  for (i = 0; i < req->nparams; i++) {
    ENVP_PARAM("KNEGDP", req->params[i]);
  }

  assert(off <= req->nparams + 2);
  return envp;

fail:
  free_envp(envp);
  return NULL;
}

static struct kng_ctx *kng_new(struct ycl_msg_knegd_req *req,
    const char **err) {
  struct kng_ctx *s = NULL;
  int fd = -1;
  int sfds[2] = {-1, -1};
  int ret;
  char path[256];
  pid_t pid = -1;
  char **envp = NULL;

  assert(req != NULL);
  assert(err != NULL);

  if (!is_valid_type(req->type)) {
    *err = "empty or invalid scan type";
    goto fail;
  }

  s = calloc(1, sizeof(struct kng_ctx));
  if (s == NULL) {
    *err = "insufficient memory";
    goto fail;
  }

  /* TODO: Get ID */
  s->id = "DUMMY";

  envp = mkenvp(req, s->id);
  if (envp == NULL) {
    *err = "insufficient memory";
    goto cleanup_s;
  }

  snprintf(path, sizeof(path), "%s/%s", YSCANSDIR, req->type);
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    *err = (errno == ENOENT) ? "no such scan type" : "scan type failure";
    goto cleanup_envp;
  }

  /* TODO: Don't feed parent process, open fd to log and /dev/null for stdin */
  ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sfds);
  if (ret < 0) {
    *err = "socketpair failure";
    goto cleanup_fd;
  }

  pid = fork();
  if (pid < 0) {
    *err = "fork failure";
    goto cleanup_socketpair;
  } else if (pid == 0) {
    char *argv[2] = {path, NULL};
    dup2(sfds[1], STDIN_FILENO);
    dup2(sfds[1], STDOUT_FILENO);
    dup2(sfds[1], STDERR_FILENO);
    close(sfds[0]);
    close(sfds[1]);
    fexecve(fd, argv, envp);
    exit(EXIT_FAILURE);
  }

  free_envp(envp);
  close(sfds[1]);
  close(fd);
  s->pid = pid;
  s->sock = sfds[0];
  return s;

cleanup_socketpair:
  close(sfds[0]);
  close(sfds[1]);
cleanup_fd:
  close(fd);
cleanup_envp:
  free_envp(envp);
cleanup_s:
  free(s);

fail:
  return NULL;
}

static void kng_free(struct kng_ctx *s) {
  if (s != NULL) {
    if (s->sock >= 0) {
      close(s->sock);
      s->sock = -1;
    }
    free(s);
  }
}

static void kng_stop(struct kng_ctx *s) {
  assert(s != NULL);
  kill(s->pid, SIGTERM);
  /* TODO: mark for removal and SIGKILL in the future if we havn't reaped the
   *       process within N seconds */
}

/* list of running scans */
struct kng_ctx *scans_;

static void scans_add(struct kng_ctx *s) {
  assert(s != NULL);
  assert(s->id != NULL);

  s->next = scans_;
  scans_ = s;
}

static struct kng_ctx *scans_find_by_id(const char *id) {
  struct kng_ctx *curr;

  assert(id != NULL);

  for (curr = scans_; curr != NULL; curr = curr->next) {
    if (strcmp(curr->id, id) == 0) {
      return curr;
    }
  }

  return NULL;
}

static const char *scans_status(const char *id) {
  assert(id != NULL);
  /* TODO: Replace "status" with "pid" and return -1 on no PID */

  if (scans_find_by_id(id)) {
    return "ACTIVE";
  }

  return "INACTIVE";
}

static void scans_stop(const char *id) {
  struct kng_ctx *curr;

  assert(id != NULL);
  curr = scans_find_by_id(id);
  if (curr != NULL) {
    kng_stop(curr);
  }
}


/* NB: Must only be called after the child is reaped by eds */
static void scans_remove(pid_t pid) {
  struct kng_ctx *curr;
  struct kng_ctx *prev;

  assert(pid > 0);

  /* find the element, if any */
  for (curr = scans_; curr != NULL; curr = curr->next) {
    if (curr->pid == pid) {
      break;
    }
    prev = curr;
  }

  if (curr == NULL) {
    return;
  } else if (curr == scans_) {
    scans_ = curr->next;
  } else {
    prev->next = curr->next;
  }

  kng_free(curr);
}

static void write_err_response(struct eds_client *cli, int fd,
    const char *errmsg) {
  struct kng_cli *ecli = KNG_CLI(cli);
  struct ycl_msg_status_resp resp = {0};
  int ret;

  LOGERR("failure: %s fd:%d", errmsg, fd);
  eds_client_clear_actions(cli);
  resp.errmsg = errmsg;
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    LOGERR("error response serialization error fd:%d", fd);
  } else {
    eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
        ycl_msg_nbytes(&ecli->msgbuf), NULL);
  }
}

static void write_ok_response(struct eds_client *cli, int fd,
    const char *msg) {
  struct kng_cli *ecli = KNG_CLI(cli);
  struct ycl_msg_status_resp resp = {0};
  int ret;

  eds_client_clear_actions(cli);
  resp.okmsg = msg;
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    LOGERR("OK response serialization error fd:%d", fd);
  } else {
    eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
        ycl_msg_nbytes(&ecli->msgbuf), NULL);
  }
}

static void start_stream_log(struct eds_client *cli, int fd,
    const char *id) {
  /* TODO: Implement */
}

static void on_readreq(struct eds_client *cli, int fd) {
  const char *errmsg = "an internal error occurred";
  const char *okmsg = "OK";
  struct kng_cli *ecli = KNG_CLI(cli);
  struct ycl_msg_knegd_req req = {0};
  struct kng_ctx *scan;
  int ret;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    errmsg = ycl_strerror(&ecli->ycl);
    goto fail;
  }

  ret = ycl_msg_parse_knegd_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    errmsg = "knegd request parse error";
    goto fail;
  }

  if (req.action == NULL) {
    errmsg = "missing action";
    goto fail;
  }

  /* check if the client wants a scan to start */
  if (strcmp(req.action, "start") == 0) {
    scan = kng_new(&req, &errmsg);
    if (scan == NULL) {
      goto fail;
    }
    scans_add(scan);
    /* TODO: add on_readable callback for scan->sock for log */
    okmsg = scan->id; /* XXX: NOT GOOD, scan may not live past sending ID */
    LOGINFO("started scan fd:%d type:\"%s\" pid:%d", fd, req.type, scan->pid);
    write_ok_response(cli, fd, okmsg);
    return;
  }

  /* anything past this point requires ID, so check it */
  if (req.id == NULL) {
    errmsg = "missing scan ID";
    goto fail;
  }

  /* check streaming actions, resulting in non-simple req/resp comm */
  if (strcmp(req.action, "log") == 0) {
    start_stream_log(cli, fd, req.id);
    return;
  }

  /* check req/resp actions, or error out on unknown action */
  if (strcmp(req.action, "status") == 0) {
    okmsg = scans_status(req.id);
  } else if (strcmp(req.action, "stop") == 0) {
    scans_stop(req.id);
  } else {
    errmsg = "unknown action";
    goto fail;
  }

  write_ok_response(cli, fd, okmsg);
  return;
fail:
  write_err_response(cli, fd, errmsg);
}

void kng_on_readable(struct eds_client *cli, int fd) {
  struct kng_cli *ecli = KNG_CLI(cli);
  int ret;

  ycl_init(&ecli->ycl, fd);
  if (ecli->flags & KNGFL_HASMSGBUF) {
    ycl_msg_reset(&ecli->msgbuf);
  } else {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      LOGERR("ycl_msg_init failure fd:%d", fd);
      goto fail;
    }
    ecli->flags |= KNGFL_HASMSGBUF;
  }
  eds_client_set_on_readable(cli, on_readreq, 0);
  return;

fail:
  eds_client_clear_actions(cli);
}

void kng_on_svc_reaped_child(struct eds_service *svc, pid_t pid,
    int status) {
  LOGINFO("scan done pid:%d status:0x%x", pid, status);
  scans_remove(pid);
}

void kng_on_done(struct eds_client *cli, int fd) {
  /* TODO: Implement */
}

void kng_on_finalize(struct eds_client *cli) {
  struct kng_cli *ecli = KNG_CLI(cli);
  if (ecli->flags & KNGFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~KNGFL_HASMSGBUF;
  }
}

void kng_mod_fini(struct eds_service *svc) {
  int nprocs = 0;
  struct kng_ctx *curr;
  struct kng_ctx *next;
  struct timespec sleep;
  struct timespec remaining;
  pid_t pid;
  int status;

  for (curr = scans_; curr != NULL; curr = curr->next) {
    kill(curr->pid, SIGTERM);
    nprocs++;
  }

  if (nprocs > 0) {
    sleep.tv_sec = 1;
    sleep.tv_nsec = 0;
    while (nanosleep(&sleep, &remaining) < 0) {
      sleep = remaining;
    }

    for (curr = scans_; curr != NULL; curr = next) {
      kill(curr->pid, SIGKILL);
      next = curr->next;
      kng_free(curr);
    }

    while (nprocs > 0) {
      pid = wait(&status);
      if (pid < 0) {
        LOGERR("shutdown wait: nprocs:%d error:\"%s\"", nprocs,
            strerror(errno));
        break;
      }
      LOGINFO("killed scan pid:%d", pid);
      nprocs--;
    }
  }
}

