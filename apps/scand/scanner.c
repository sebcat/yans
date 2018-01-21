#include <sys/types.h>
#include <sys/socket.h>
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
#include <apps/scand/scanner.h>

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

#define SCANNERFL_HASMSGBUF (1 << 0)

#define LOGERR(fd__, ...) \
    ylog_error("scannercli%d: %s", (fd__), __VA_ARGS__)

#define LOGINFOF(fd__, fmt, ...) \
    ylog_info("scannercli%d: " fmt, (fd__), __VA_ARGS__)

#define MAX_ENVPARAMS 128

struct scan_ctx {
  struct scan_ctx *next;
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
    for (i = 0; i < MAX_ENVPARAMS; i++) {
      if (envp[i] == NULL) {
        break;
      }
      free(envp[i]);
    }
    free(envp);
  }
}

static char *constr(const char *name, const char *value) {
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

#define ENVP_FIELD(name__, field__)          \
  if ((field__) != NULL) {                   \
    envp[off] = constr((name__), (field__)); \
    if (envp[off] == NULL) {                 \
      goto fail;                             \
    }                                        \
    off++;                                   \
  }

/* build the environment from the scand request */
static char **mkenvp(struct ycl_msg_scand_req *req) {
  size_t off = 0;
  char **envp;

  envp = calloc(MAX_ENVPARAMS, sizeof(char*));
  if (envp == NULL) {
    return NULL;
  }

  /* NB: If you add something, make sure you do not exceed MAX_ENVPARAMS */
  ENVP_FIELD("SCAND_ID", req->id);
  ENVP_FIELD("SCAND_TYPE", req->type);
  ENVP_FIELD("SCAND_TARGETS", req->targets);
  ENVP_FIELD("SCAND_TCP_PORTS", req->tcp_ports);
  assert(off < MAX_ENVPARAMS);
  return envp;

fail:
  free_envp(envp);
  return NULL;
}

static struct scan_ctx *scan_new(struct ycl_msg_scand_req *req,
    const char **err) {
  struct scan_ctx *s = NULL;
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

  s = calloc(1, sizeof(struct scan_ctx));
  envp = mkenvp(req);
  if (s == NULL || envp == NULL) {
    *err = "insufficient memory";
    goto fail;
  }

  snprintf(path, sizeof(path), "%s/%s", YSCANSDIR, req->type);
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    *err = (errno == ENOENT) ? "no such scan type" : "scan type failure";
    goto fail;
  }

  /* TODO: Get ID */
  s->id = "DUMMY";

  ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sfds);
  if (ret < 0) {
    *err = "socketpair failure";
    goto fail;
  }

  pid = fork();
  if (pid < 0) {
    *err = "fork failure";
    goto fail;
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

fail:
  if (s != NULL) {
    free(s);
  }

  if (envp != NULL) {
    free_envp(envp);
  }

  if (fd >= 0) {
    close(fd);
  }

  if (sfds[0] >= 0) {
    close(sfds[0]);
  }

  if (sfds[1] >= 0) {
    close(sfds[1]);
  }

  return NULL;
}

static void scan_free(struct scan_ctx *s) {
  if (s != NULL) {
    if (s->sock >= 0) {
      close(s->sock);
      s->sock = -1;
    }
    free(s);
  }
}

static void scan_stop(struct scan_ctx *s) {
  assert(s != NULL);
  kill(s->pid, SIGTERM);
  /* TODO: mark for removal and SIGKILL in the future if we havn't reaped the
   *       process within N seconds */
}

/* list of running scans */
struct scan_ctx *scans_;

static void scans_add(struct scan_ctx *s) {
  assert(s != NULL);
  assert(s->id != NULL);

  s->next = scans_;
  scans_ = s;
}

static struct scan_ctx *scans_find_by_id(const char *id) {
  struct scan_ctx *curr;

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
  struct scan_ctx *curr;

  assert(id != NULL);
  curr = scans_find_by_id(id);
  if (curr != NULL) {
    scan_stop(curr);
  }
}


/* NB: Must only be called after the child is reaped by eds */
static void scans_remove(pid_t pid) {
  struct scan_ctx *curr;
  struct scan_ctx *prev;

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

  scan_free(curr);
}

static void write_err_response(struct eds_client *cli, int fd,
    const char *errmsg) {
  struct scanner_cli *ecli = SCANNER_CLI(cli);
  struct ycl_msg_status_resp resp = {0};
  int ret;

  LOGERR(fd, errmsg);
  eds_client_clear_actions(cli);
  resp.errmsg = errmsg;
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    LOGERR(fd, "error response serialization error");
  } else {
    eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
        ycl_msg_nbytes(&ecli->msgbuf), NULL);
  }
}

static void write_ok_response(struct eds_client *cli, int fd,
    const char *msg) {
  struct scanner_cli *ecli = SCANNER_CLI(cli);
  struct ycl_msg_status_resp resp = {0};
  int ret;

  eds_client_clear_actions(cli);
  resp.okmsg = msg;
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    LOGERR(fd, "OK response serialization error");
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
  struct scanner_cli *ecli = SCANNER_CLI(cli);
  struct ycl_msg_scand_req req = {0};
  struct scan_ctx *scan;
  int ret;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    errmsg = ycl_strerror(&ecli->ycl);
    goto fail;
  }

  ret = ycl_msg_parse_scand_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    errmsg = "scand request parse error";
    goto fail;
  }

  if (req.action == NULL) {
    errmsg = "missing action";
    goto fail;
  }

  /* check if the client wants a scan to start */
  if (strcmp(req.action, "start") == 0) {
    scan = scan_new(&req, &errmsg);
    if (scan == NULL) {
      goto fail;
    }
    scans_add(scan);
    /* TODO: add on_readable callback for scan->sock for log */
    okmsg = scan->id; /* XXX: NOT GOOD, scan may not live past sending ID */
    LOGINFOF(fd, "started scan type:\"%s\" pid:%d", req.type, scan->pid);
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

void scanner_on_readable(struct eds_client *cli, int fd) {
  struct scanner_cli *ecli = SCANNER_CLI(cli);
  int ret;

  ycl_init(&ecli->ycl, fd);
  if (ecli->flags & SCANNERFL_HASMSGBUF) {
    ycl_msg_reset(&ecli->msgbuf);
  } else {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      LOGERR(fd, "ycl_msg_init failure");
      goto fail;
    }
    ecli->flags |= SCANNERFL_HASMSGBUF;
  }
  eds_client_set_on_readable(cli, on_readreq, 0);
  return;

fail:
  eds_client_clear_actions(cli);
}

void scanner_on_svc_reaped_child(struct eds_service *svc, pid_t pid,
    int status) {
  ylog_info("scan done pid:%d status:0x%x", pid, status);
  scans_remove(pid);
}

void scanner_on_done(struct eds_client *cli, int fd) {
  /* TODO: Implement */
}

void scanner_on_finalize(struct eds_client *cli) {
  struct scanner_cli *ecli = SCANNER_CLI(cli);
  if (ecli->flags & SCANNERFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~SCANNERFL_HASMSGBUF;
  }
}

void scanner_mod_fini(struct eds_service *svc) {
  /* TODO: we need to kill all the children gracefully (SIGTERM, sleep,
   *       reap, SIGKILL what's left) */
}

