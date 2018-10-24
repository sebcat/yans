#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include <fts.h>

#include <lib/util/macros.h>
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

#define KNGQ_IDSIZE 48
#define KNGQ_TYPESIZE 256

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
  char *queuedir;
  int nqueueslots;
  time_t timeout;
  const char *storesock;
};

struct kng_opts opts_ = {
  .knegdir = DFL_KNEGDIR,
  .queuedir = DFL_QUEUEDIR,
  .nqueueslots = DFL_NQUEUESLOTS,
  .timeout = DFL_TIMEOUT,
  .storesock = DFL_STORESOCK,
};

/* persistent work queue */
struct kng_queue {
  char *basepath;              /* queue root directory */
  int err;                     /* saved errno, if any */
  int nslots;                  /* total number of slots */
  int nrunning;                /* number of occupied slots */
  FTS *fts;                    /* current fts context, if any */
  char id[KNGQ_IDSIZE];        /* saved ID, valid for one iteration */
  char type[KNGQ_TYPESIZE];    /* saved type, valid for one iteration */
};

struct kng_ctx *jobs_;    /* list of running jobs */
struct kng_queue kngq_;  /* work queue instance */

static inline int kngq_nslots(struct kng_queue *kngq) {
  return kngq->nslots;
}

static inline int kngq_nrunning(struct kng_queue *kngq) {
  return kngq->nrunning;
}

static inline void kngq_update_running(struct kng_queue *kngq, int diff) {
  kngq->nrunning += diff;
}

static int kngq_init(struct kng_queue *kngq, char *basepath, int nslots) {
  int ret;

  assert(basepath != NULL);

  kngq->basepath = basepath;
  kngq->nslots = nslots;
  ret = mkdir(kngq->basepath, 0700);
  if (ret != 0 && errno != EEXIST) {
    kngq->err = errno;
    return -1;
  }

  return 0;
}

static const char *kngq_strerror(struct kng_queue *kngq) {
  if (kngq->err == 0) {
    return "unknown error";
  } else {
    return strerror(kngq->err);
  }
}

static int kngq_put(struct kng_queue *kngq, const char *type,
    const char *id) {
  char path[1024];
  char timestr[32];
  size_t timelen;
  int ret;

  /* get a string of the current time in hex, at least 8 chars wide */
  snprintf(timestr, sizeof(timestr), "%.08lx", time(NULL));
  timelen = strlen(timestr);

  /* get a string of the subdir where to put the entry and create it */
  snprintf(path, sizeof(path), "%s/%c%c", kngq->basepath,
      timestr[timelen-4], timestr[timelen-3]);
  ret = mkdir(path, 0700);
  if (ret != 0 && errno != EEXIST) {
    kngq->err = errno;
    return -1;
  }

  /* create the entry path and create the file */ 
  snprintf(path, sizeof(path), "%s/%c%c/%s-%s-%s", kngq->basepath,
      timestr[timelen-4], timestr[timelen-3], timestr, id, type);
  ret = open(path, O_WRONLY|O_CREAT, 0600);
  if (ret < 0) {
    kngq->err = errno;
    return -1;
  }

  close(ret);
  return 0;
}

static int fts_oldest_first(const FTSENT * const *a,
    const FTSENT * const *b) {
  if ((*a)->fts_info == FTS_NS || (*a)->fts_info == FTS_NSOK ||
      (*b)->fts_info == FTS_NS || (*b)->fts_info == FTS_NSOK) {
    /* fallback on string comparison of name */
    return strcmp((*a)->fts_name, (*b)->fts_name);
  }

  return  (*a)->fts_statp->st_mtim.tv_sec -
      (*b)->fts_statp->st_mtim.tv_sec;
}

static FTSENT *_kngq_next_file(struct kng_queue *kngq) {
  FTSENT *ent;

  while ((ent = fts_read(kngq->fts)) != NULL) {
    if (ent->fts_info == FTS_F) {
      break;
    } else if (ent->fts_info == FTS_DP && ent->fts_level >= 1) {
      /* post-order subdir - try to rmdir it. If it's empty - good. If
       * it's not - no problem */
      rmdir(ent->fts_path);
    }
  }

  return ent;
}

static FTSENT *kngq_next_file(struct kng_queue *kngq) {
  char *paths[] = {
    kngq->basepath, NULL
  };
  FTSENT *ent;
  int retry = 1;

  /* open the fts context, if none exists. If an fts context does not exist
   * and _kngq_next_file  return NULL, there's no need to retry a second
   * time - there are no jobs in the queue */
  if (kngq->fts == NULL) {
    kngq->fts = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR,
        fts_oldest_first);
    retry = 0;
  }

  /* iterate over the entries until end or found file */
  ent = _kngq_next_file(kngq);

  /* if we've reached the end, reopen once and try again */
  if (ent == NULL && retry) {
    fts_close(kngq->fts);
    kngq->fts = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR,
        fts_oldest_first);
    ent = _kngq_next_file(kngq);
    if (ent == NULL) {
      fts_close(kngq->fts);
      kngq->fts = NULL;
    }
  }

  return ent;
}

/* return -1 on error, 0 on success */ 
static int kngq_get_fields(struct kng_queue *kngq, const char *str,
  const char **id, const char **type) {
  const char *curr;
  const char *next;
  size_t len;

  /*      curr
   *         v       
   * timestr-id-type */
  curr = strchr(str, '-');
  if (curr == NULL || *(++curr) == '\0') {
    return -1;
  }

  /*      curr next
   *         v v
   * timestr-id-type */
  next = strchr(curr, '-');
  if (next == NULL) {
    return -1;
  }
  len = MIN(sizeof(kngq->id), next-curr);
  strncpy(kngq->id, curr, next-curr);
  kngq->id[sizeof(kngq->id)-1] = '\0';

  /*            curr
   *            v
   * timestr-id-type */
  curr = next + 1;
  if (*curr == '\0') {
    return -1;
  }
  snprintf(kngq->type, sizeof(kngq->type), "%s", curr);

  *id = kngq->id;
  *type = kngq->type;
  return 0;
}

/* returns -1 on error, 0 on no elements in queue, 1 on returned element.
 * The returned element will be the one of the oldest mtime - meaning
 * the oldest entry unless we wrap around and start placing new entries
 * in subdirs which have not yet been depleted. This may be problematic */
static int kngq_next(struct kng_queue *kngq, const char **id,
    const char **type) {
  int ret;
  FTSENT *ent;

  /* get the FTS entry of the next file in the queue, if any */
  ent = kngq_next_file(kngq);
  if (ent == NULL) {
    return 0;
  }

  /* remove the file to avoid double queueing */
  unlink(ent->fts_path);

  /* Get the id and the type fields and copy them to kng_queue.
   * ent->fts_name should be of format timestr-id-type. */
  ret = kngq_get_fields(kngq, ent->fts_name, id, type);
  if (ret < 0) {
    kngq->err = EINVAL;
    return -1;
  }
 
  return 1;
}

void kng_set_knegdir(const char *dir) {
  opts_.knegdir = dir;
}

void kng_set_queuedir(char *dir) {
  opts_.queuedir = dir;
}

void kng_set_nqueueslots(int nslots) {
  opts_.nqueueslots = nslots;
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

/* known, constant names */
#define ENVP_VAR(name__, value__) \
    if ((value__) != NULL && *(value__) != '\0') {   \
      envp[off] = consk((name__), (value__));        \
      if (envp[off] == NULL) {                       \
        goto fail;                                   \
      }                                              \
      off++;                                         \
    }

/* build the environment from the knegd request */
static char **mkenvp(const char *id, const char *type) {
  size_t off = 0;
  char **envp;

  /* must be: number of ENVP_VARs + NULL sentinel */
  envp = calloc(4, sizeof(char*));
  if (envp == NULL) {
    return NULL;
  }

  ENVP_VAR("PATH", "/usr/local/bin:/bin:/usr/bin");
  ENVP_VAR("YANS_ID", id);
  ENVP_VAR("YANS_TYPE", type);

  assert(off <= 3);
  return envp;

fail:
  free_envp(envp);
  return NULL;
}

struct kng_jobopts {
  const char *type;    /* kneg type */
  const char *id;      /* job ID, if any */
  const char *name;    /* name for stored index, if any */
  time_t timeout_sec;  /* timeout in seconds, or 0 for no timeout */
};

static struct kng_ctx *kng_new(struct kng_jobopts *opts,
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

  assert(opts != NULL);
  assert(err != NULL);

  if (!is_valid_type(opts->type)) {
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
  storereqmsg.store_id.len = opts->id ? strlen(opts->id) : 0;
  storereqmsg.store_id.data = opts->id;
  storereqmsg.name.len = opts->name ? strlen(opts->name) : 0;
  storereqmsg.name.data =opts->name;
  storereqmsg.indexed = (long)time(NULL);
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
  envp = mkenvp(s->id, opts->type);
  if (envp == NULL) {
    *err = "insufficient memory";
    goto cleanup_ycl_msg;
  }

  snprintf(path, sizeof(path), "%s/%s", opts_.knegdir, opts->type);
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

  if (opts->timeout_sec > 0) {
    s->timeout = opts->timeout_sec;
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
      fprintf(stderr, "%s: %s\n", path, strerror(errno));
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
  struct kng_ctx *prev = NULL;

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
    eds_client_clear_actions(cli);
    return;
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
    struct kng_jobopts opts = {
      .type = req.type.data,
      .id = req.id.data,
      .name = req.name.data,
      .timeout_sec = req.timeout > 0 ? (time_t)req.timeout : 0,
    };

    job = kng_new(&opts, &errmsg);
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
    errmsg = "missing id";
    goto fail;
  }

  /* check req/resp actions, or error out on unknown action */
  if (strcmp(req.action.data, "queue") == 0) {
    if (!is_valid_type(req.type.data)) {
      errmsg = "empty or invalid job type";
      goto fail;
    }

    if (req.id.len == 0) {
      errmsg = "missing id";
      goto fail;
    }

    if (strchr(req.id.data, '/') != NULL) {
      errmsg = "invalid id";
      goto fail;
    }

    ret = kngq_put(&kngq_, req.type.data, req.id.data);
    if (ret != 0) {
      errmsg = kngq_strerror(&kngq_);
      LOGERR("queue request failed id:\"%s\" type:\"%s\" %s", req.id.data,
          req.type.data, errmsg);
      goto fail;
    } else {
      LOGINFO("queued id:\"%s\" type:\"%s\"", req.id.data, req.type.data);
    }
  } else if (strcmp(req.action.data, "pids") == 0) {
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

int kng_mod_init(struct eds_service *svc) {
  int ret;

  ret = kngq_init(&kngq_, opts_.queuedir, opts_.nqueueslots);
  if (ret < 0) {
    fprintf(stderr, "kng_mod_init: %s\n", kngq_strerror(&kngq_));
    return -1;
  }

  return 0;
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

static void dispatch_n_jobs(int n) {
  int ret;
  const char *id;
  const char *type;

  while (n > 0) {
    ret = kngq_next(&kngq_, &id, &type);
    if (ret < 0) {
      LOGERR("kngq_next: %s", kngq_strerror(&kngq_));
      break;
    } else if (ret == 0) {
      break;
    }

    LOGINFO("TODO: dispatch %s %s", id, type);
    /* we should update nrunning for each dispatched job, and update it
     * again when we reap the queued job */
    kngq_update_running(&kngq_, 0); /* TODO: update diff */
    n--;
  }
}

void kng_on_tick(struct eds_service *svc) {
  int nslots;
  int nrunning;

  /* check if any job has exceeded their allocated execution time */
  jobs_check_times();

  /* start queued jobs, if any */
  nslots = kngq_nslots(&kngq_);
  nrunning = kngq_nrunning(&kngq_);
  if (nrunning < nslots) {
    dispatch_n_jobs(nslots - nrunning);
  }
}
