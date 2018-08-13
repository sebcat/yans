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

#include <lib/util/str.h>
#include <lib/util/ylog.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/knegd/kng.h>

/* valid characters for job types - must not contain path chars &c */
#define VALID_TYPECH(ch__) \
  (((ch__) >= 'a' && (ch__) <= 'z') || \
   ((ch__) >= '0' && (ch__) <= '9') || \
   ((ch__) == '-'))

#define KNGFL_HASMSGBUF  (1 << 0)
#define KNGFL_HASRESPBUF (1 << 1)

#define DFL_RESPBUFSZ 1024

#define LOGERR(...) \
    ylog_error(__VA_ARGS__)

#define LOGINFO(...) \
    ylog_info(__VA_ARGS__)

#define KNG_SENTSIGTERM (1 << 0)
#define KNG_STOPPED     (1 << 1)

struct kng_ctx {
  struct kng_ctx *next;
  int flags;
  struct timespec started; /* CLOCK_MONOTONIC time of start */
  time_t timeout;
  pid_t pid;
  char id[48];
};

struct kng_opts {
  const char *knegdir;
  time_t timeout;
  const char *storesock;
};

/* list of running jobs */
struct kng_ctx *jobs_;

struct kng_opts opts_ = {
  .knegdir = DFL_KNEGDIR,
  .timeout = DFL_TIMEOUT,
  .storesock = DFL_STORESOCK,
};

void kng_set_knegdir(const char *dir) {
  opts_.knegdir = dir;
}

void kng_set_storesock(const char *path) {
  opts_.storesock = path;
}

void kng_set_timeout(long timeout) {
  if (timeout > 0) {
    opts_.timeout = (time_t)timeout;
  }
}

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

  /* must be: number of ENVP_PARAM + number of ENVP_VARs + NULL sentinel */
  envp = calloc(req->nparams + 4, sizeof(char*));
  if (envp == NULL) {
    return NULL;
  }

  ENVP_VAR("PATH", "/usr/local/bin:/bin:/usr/bin");
  ENVP_VAR("YANS_ID", id);
  ENVP_VAR("YANS_TYPE", req->type.data);
  for (i = 0; i < req->nparams; i++) {
    ENVP_PARAM("YANSP", req->params[i].data);
  }

  assert(off <= req->nparams + 3);
  return envp;

fail:
  free_envp(envp);
  return NULL;
}

static struct kng_ctx *kng_new(struct ycl_msg_knegd_req *req,
    const char **err) {
  struct kng_ctx *s = NULL;
  int logfd;
  int ret;
  int closed_ycls = 0;
  char path[256];
  pid_t pid = -1;
  char **envp = NULL;
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_req storereqmsg = {{0}};
  struct ycl_msg_store_entered_req enteredmsg = {{0}};
  struct ycl_msg_status_resp resp = {{0}};

  assert(req != NULL);
  assert(err != NULL);

  if (!is_valid_type(req->type.data)) {
    *err = "empty or invalid job type";
    goto fail;
  }

  s = calloc(1, sizeof(struct kng_ctx));
  if (s == NULL) {
    *err = "insufficient memory";
    goto fail;
  }

  /* we do a fresh connect for every new job to avoid transient errors
   * affecting too much */
  ret = ycl_connect(&ctx, opts_.storesock);
  if (ret != YCL_OK) {
    *err = "store connection failure";
    goto cleanup_s;
  }

  ret = ycl_msg_init(&msg);
  if (ret != YCL_OK) {
    *err = "ycl msg initialization failure";
    goto cleanup_ycl;
  }

  storereqmsg.action.data = "enter";
  storereqmsg.action.len = sizeof("enter")-1;
  storereqmsg.store_id.len = req->id.len;
  storereqmsg.store_id.data = req->id.data;
  storereqmsg.name.len = req->name.len;
  storereqmsg.name.data = req->name.data;
  ret = ycl_msg_create_store_req(&msg, &storereqmsg);
  if (ret != YCL_OK) {
    *err = "enter request serialization error";
    goto cleanup_ycl_msg;
  }

  ret = ycl_sendmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    *err = "failed to send enter request";
    goto cleanup_ycl_msg;
  }

  ycl_msg_reset(&msg);
  ret = ycl_recvmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    *err = "failed to receive enter response";
    goto cleanup_ycl_msg;
  }

  ret = ycl_msg_parse_status_resp(&msg, &resp);
  if (ret != YCL_OK) {
    *err = "failed to parse enter response";
    goto cleanup_ycl_msg;
  }

  if (resp.errmsg.data != NULL && *resp.errmsg.data != '\0') {
    *err = "enter request failed";
    goto cleanup_ycl_msg;
  }

  if (resp.okmsg.data == NULL || *resp.okmsg.data == '\0') {
    *err = "failed to receive store ID";
    goto cleanup_ycl_msg;
  }

  strncpy(s->id, resp.okmsg.data, sizeof(s->id));
  s->id[sizeof(s->id)-1] = '\0';
  envp = mkenvp(req, s->id);
  if (envp == NULL) {
    *err = "insufficient memory";
    goto cleanup_ycl_msg;
  }

  snprintf(path, sizeof(path), "%s/%s", opts_.knegdir, req->type.data);
  enteredmsg.action.data = "open";
  enteredmsg.action.len = 5;
  enteredmsg.open_path.data = "kneg.log";
  enteredmsg.open_path.len = strlen(enteredmsg.open_path.data);
  enteredmsg.open_flags = O_WRONLY|O_CREAT|O_TRUNC;
  ret = ycl_msg_create_store_entered_req(&msg, &enteredmsg);
  if (ret != YCL_OK) {
    *err = "failed to create store open message";
    goto cleanup_envp;
  }

  ret = ycl_sendmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    *err = "failed to send store open message";
    goto cleanup_envp;
  }

  ret = ycl_recvfd(&ctx, &logfd);
  if (ret != YCL_OK) {
    LOGERR("store open: %s\n", ycl_strerror(&ctx));
    *err = "failed to open log file";
    goto cleanup_envp;
  }

  ret = clock_gettime(CLOCK_MONOTONIC, &s->started);
  if (ret < 0) {
    *err = "clock_gettime failure";
    goto cleanup_logfd;
  }

  if (req->timeout > 0) {
    s->timeout = (time_t)req->timeout;
  } else {
    s->timeout = opts_.timeout;
  }

  /* NB: We cleanup before fork because we dont want to inherit the ycl fd */
  ycl_msg_cleanup(&msg);
  ycl_close(&ctx);
  closed_ycls = 1;

  pid = fork();
  if (pid < 0) {
    *err = "fork failure";
    goto cleanup_logfd;
  } else if (pid == 0) {
    char *argv[2] = {path, NULL};
    dup2(logfd, STDIN_FILENO);
    dup2(logfd, STDOUT_FILENO);
    dup2(logfd, STDERR_FILENO);
    close(logfd);
    /* client sockets should be CLOEXEC, but cmdfd and potentially others
     * are not. A bit hacky, but... */
    for (ret = 3; ret < 10; ret++) {
      close(ret); 
    }
    ret = execve(path, argv, envp);
    if (ret < 0) {
      fprintf(stderr, "execve: %s\n", strerror(errno));
    }
    _exit(EXIT_FAILURE);
  }

  free_envp(envp);
  close(logfd);
  s->pid = pid;
  return s;

cleanup_logfd:
  close(logfd);
cleanup_envp:
  free_envp(envp);
cleanup_ycl_msg:
  if (!closed_ycls) {
    ycl_msg_cleanup(&msg);
  }
cleanup_ycl:
  if (!closed_ycls) {
    ycl_close(&ctx);
  }
cleanup_s:
  free(s);

fail:
  return NULL;
}

static void kng_free(struct kng_ctx *s) {
  if (s != NULL) {
    free(s);
  }
}

static void kng_stop(struct kng_ctx *s) {
  assert(s != NULL);
  kill(s->pid, SIGTERM);
  s->flags |= KNG_STOPPED;
}

static void jobs_add(struct kng_ctx *s) {
  assert(s != NULL);
  assert(s->id != NULL);

  s->next = jobs_;
  jobs_ = s;
}

static struct kng_ctx *jobs_find_by_id(const char *id) {
  struct kng_ctx *curr;

  assert(id != NULL);

  for (curr = jobs_; curr != NULL; curr = curr->next) {
    if (strcmp(curr->id, id) == 0) {
      return curr;
    }
  }

  return NULL;
}

static void jobs_check_times() {
  struct kng_ctx *curr;
  struct timespec tv;
  int ret;
  time_t nsecs;

  ret = clock_gettime(CLOCK_MONOTONIC, &tv);
  if (ret < 0) {
    LOGERR("jobs_check_times: clock_gettime: %s", strerror(errno));
    return;
  }

  for (curr = jobs_; curr != NULL; curr = curr->next) {
    if (curr->flags & KNG_SENTSIGTERM) {
      LOGERR("sending SIGKILL to pid:%d", curr->pid);
      kill(curr->pid, SIGKILL);
      continue;
    }

    if (curr->flags & KNG_STOPPED) {
      /* if reached, we've at a recent past sent SIGTERM due to a user
       * stopping the job. We don't want to SIGKILL it the first time we tick
       * because that may be immediately after the stopping action, so we
       * set KNG_SENTSIGTERM at the first tick instead, and send SIGKILL on
       * the next tick if we're still alive */
      curr->flags |= KNG_SENTSIGTERM;
      continue;
    }

    nsecs = tv.tv_sec - curr->started.tv_sec;
    if (nsecs >= curr->timeout) {
      LOGERR("timeout reached pid:%d", curr->pid);
      kill(curr->pid, SIGTERM);
      curr->flags |= KNG_SENTSIGTERM;
      continue;
    }
  }
}


static pid_t jobs_pid(const char *id) {
  struct kng_ctx *s;
  assert(id != NULL);

  s = jobs_find_by_id(id);
  if (s != NULL) {
    return s->pid;
  }

  return -1;
}

int append_job_pid(const char *s, size_t len, void *data) {
  char pidstr[32];
  buf_t *buf = data;

  if (buf->len > 0) {
    buf_achar(buf, ' ');
  }

  snprintf(pidstr, sizeof(pidstr), "%d", jobs_pid(s));
  buf_adata(buf, pidstr, strlen(pidstr));
  return 1;
}

static void append_job_pids(buf_t *buf, const char *s, size_t len) {
  str_map_field(s, len, "", 1, append_job_pid, buf);
  buf_achar(buf, '\0');
}

static void jobs_stop(const char *id) {
  struct kng_ctx *curr;

  assert(id != NULL);
  curr = jobs_find_by_id(id);
  if (curr != NULL) {
    kng_stop(curr);
  }
}

/* NB: Must only be called after the child is reaped by eds */
static void jobs_remove(pid_t pid) {
  struct kng_ctx *curr;
  struct kng_ctx *prev;

  assert(pid > 0);

  /* find the element, if any */
  for (curr = jobs_; curr != NULL; curr = curr->next) {
    if (curr->pid == pid) {
      break;
    }
    prev = curr;
  }

  if (curr == NULL) {
    return;
  } else if (curr == jobs_) {
    jobs_ = curr->next;
  } else {
    prev->next = curr->next;
  }

  kng_free(curr);
}

static void write_err_response(struct eds_client *cli, int fd,
    const char *errmsg) {
  struct kng_cli *ecli = KNG_CLI(cli);
  struct ycl_msg_status_resp resp = {{0}};
  int ret;

  LOGERR("failure: %s fd:%d", errmsg, fd);
  eds_client_clear_actions(cli);
  resp.errmsg.data = errmsg;
  resp.errmsg.len = strlen(errmsg);
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
  struct ycl_msg_status_resp resp = {{0}};
  int ret;

  eds_client_clear_actions(cli);
  resp.okmsg.data = msg;
  resp.okmsg.len = strlen(resp.okmsg.data);
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    LOGERR("OK response serialization error fd:%d", fd);
  } else {
    eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
        ycl_msg_nbytes(&ecli->msgbuf), NULL);
  }
}

static void on_readreq(struct eds_client *cli, int fd) {
  const char *errmsg = "an internal error occurred";
  const char *okmsg = "OK";
  struct kng_cli *ecli = KNG_CLI(cli);
  struct ycl_msg_knegd_req req = {0};
  struct kng_ctx *job;
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

  if (req.action.data == NULL || *req.action.data == '\0') {
    errmsg = "missing action";
    goto fail;
  }

  /* check if the client wants a job to start */
  if (strcmp(req.action.data, "start") == 0) {
    job = kng_new(&req, &errmsg);
    if (job == NULL) {
      goto fail;
    }
    jobs_add(job);
    buf_adata(&ecli->respbuf, job->id, strlen(job->id) + 1);
    LOGINFO("started job fd:%d type:\"%s\" pid:%d id:\"%s\"", fd,
        req.type.data, job->pid, job->id);
    write_ok_response(cli, fd, ecli->respbuf.data);
    return;
  }

  /* anything past this point requires ID, so check it */
  if (req.id.data == NULL || *req.id.data == '\0') {
    errmsg = "missing job ID";
    goto fail;
  }

  /* check req/resp actions, or error out on unknown action */
  if (strcmp(req.action.data, "pids") == 0) {
    append_job_pids(&ecli->respbuf, req.id.data, req.id.len);
    okmsg = ecli->respbuf.data;
  } else if (strcmp(req.action.data, "stop") == 0) {
    jobs_stop(req.id.data);
    LOGINFO("stop request received id:\"%s\"", req.id.data);
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

  if (ecli->flags & KNGFL_HASRESPBUF) {
    buf_clear(&ecli->respbuf);
  } else {
    buf_init(&ecli->respbuf, DFL_RESPBUFSZ);
    ecli->flags |= KNGFL_HASRESPBUF;
  }

  eds_client_set_on_readable(cli, on_readreq, 0);
  return;

fail:
  eds_client_clear_actions(cli);
}

void kng_on_svc_reaped_child(struct eds_service *svc, pid_t pid,
    int status) {
  int code;

  /* NB: anything but exit code 0 is logged as an error */
  if (WIFEXITED(status)) {
    code = WEXITSTATUS(status);
    if (code == 0) {
      LOGINFO("job exited code:0 pid:%d", pid);
    } else {
      LOGERR("job exited code:%d pid:%d", code, pid);
    }
  } else if (WIFSIGNALED(status)) {
    LOGERR("job terminated signal:%d pid:%d", WTERMSIG(status), pid);
  } else {
    LOGERR("job terminated pid:%d status:0x%x", pid, status);
  }
  jobs_remove(pid);
}

void kng_on_finalize(struct eds_client *cli) {
  struct kng_cli *ecli = KNG_CLI(cli);
  if (ecli->flags & KNGFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~KNGFL_HASMSGBUF;
  }

  if (ecli->flags & KNGFL_HASRESPBUF) {
    buf_cleanup(&ecli->respbuf);
    ecli->flags &= ~KNGFL_HASRESPBUF;
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

  for (curr = jobs_; curr != NULL; curr = curr->next) {
    kill(curr->pid, SIGTERM);
    nprocs++;
  }

  if (nprocs > 0) {
    sleep.tv_sec = 1;
    sleep.tv_nsec = 0;
    while (nanosleep(&sleep, &remaining) < 0) {
      sleep = remaining;
    }

    for (curr = jobs_; curr != NULL; curr = next) {
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
      LOGINFO("killed job pid:%d", pid);
      nprocs--;
    }
  }
}

void kng_on_tick(struct eds_service *svc) {
  jobs_check_times();
}
