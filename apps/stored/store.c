#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <lib/util/ylog.h>
#include <lib/util/prng.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/stored/nullfd.h>
#include <apps/stored/store.h>

#define STORE_PATH "store"

#define STOREFL_HASMSGBUF (1 << 0)

#define MAXTRIES_GENSTORE 128

#define LOGERR(fd, ...) \
    ylog_error("storecli%d: %s", (fd), __VA_ARGS__)

#define LOGERRF(fd, fmt, ...) \
    ylog_info("storecli%d: " fmt, (fd), __VA_ARGS__)

#define LOGINFO(fd, ...) \
    ylog_info("storecli%d: %s", (fd), __VA_ARGS__)

#define LOGINFOF(fd, fmt, ...) \
    ylog_info("storecli%d: " fmt, (fd), __VA_ARGS__)

static struct prng_ctx g_prng;

static void gen_store_path(char *data, size_t len) {
  if (data == NULL || len == 0) {
    return;
  }

  prng_hex(&g_prng, data, len-1);
  data[len-1] = '\0';
}

int store_init(struct eds_service *svc) {
  time_t t;
  uint32_t seed;
  int ret;

  /* create the store subdirectory (unless it exists) or error out */
  ret = mkdir(STORE_PATH, 0700);
  if (ret < 0 && errno != EEXIST) {
    ylog_error("store: failed to create \"" STORE_PATH "\" directory: %s",
        strerror(errno));
    goto fail;
  }

  /* chdir to the store subdir or error out */
  ret = chdir(STORE_PATH);
  if (ret < 0) {
    ylog_error("store: chdir \"" STORE_PATH "\" failure: %s", strerror(errno));
    goto fail;
  }

  /* we use the PRNG for directory/file names, so it should be fine seeding
   * it with the current time (famous last words) */
  t = time(NULL);
  if (t == (time_t)-1) {
    ylog_error("store: failed to obtain initial seed (%s)", strerror(errno));
    goto fail;
  }

  seed = (uint32_t)t ^ getpid();
  prng_init(&g_prng, seed);
  ylog_info("store: initialized with seed: %u", seed);
  return 0;

fail:
  return -1;
}

static void write_err_response(struct eds_client *cli, int fd,
    const char *errmsg) {
  struct store_cli *ecli = STORE_CLI(cli);
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

static int is_valid_store_id(const char *store_id, size_t len) {
  int i;
  char ch;

  if (store_id == NULL || len != STORE_IDSZ) {
    return 0;
  }

  for (i = 0; i < len; i++) {
    ch = store_id[i];
    if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f')) {
      return 0;
    }
  }

  return 1;
}

static int is_valid_path(const char *path, size_t pathlen) {
  size_t i;
  char ch;

  if (path == NULL || pathlen >= STORE_MAXPATH) {
    return 0;
  }

  for (i = 0; i < pathlen; i++) {
    ch = path[i];
    if (ch == '/' || ch < 0x20) {
      return 0;
    }
  }

  return 1;
}

static int enter_store(struct store_cli *ecli, const char *store_id,
    int exclusive) {
  const char *subdir;
  size_t id_len;
  int ret;

  id_len = strlen(store_id);
  if (!is_valid_store_id(store_id, id_len)) {
    return -1;
  }

  subdir = store_id + STORE_IDSZ - STORE_PREFIXSZ;
  ret = mkdir(subdir, 0770);
  if (ret != 0 && errno != EEXIST) {
    return -1;
  }

  snprintf(ecli->store_path, sizeof(ecli->store_path), "%s/%s",
      subdir, store_id);
  ret = mkdir(ecli->store_path, 0770);
  if (ret != 0 && (exclusive || errno != EEXIST)) {
    return -1;
  }

  return 0;
}

static int create_and_enter_store(struct store_cli *ecli) {
  char store_id[STORE_IDSZ+1];
  int i;
  int ret;

  for (i = 0; i < MAXTRIES_GENSTORE; i++) {
    gen_store_path(store_id, sizeof(store_id));
    ret = enter_store(ecli, store_id, 1);
    if (ret == 0) {
      break;
    }
  }

  return ret;
}

static void on_readopen(struct eds_client *cli, int fd);

static void on_sendfd(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  int ret;

  ret = ycl_sendfd(&ecli->ycl, ecli->open_fd, ecli->open_errno);
  if (ret == YCL_AGAIN) {
    return;
  }

  if (ecli->open_fd != nullfd_get()) {
    close(ecli->open_fd);
  }

  if (ret != YCL_OK) {
    LOGERRF(fd, "%s: err sendfd: %s", STORE_ID(ecli),
        ycl_strerror(&ecli->ycl));
    eds_client_clear_actions(cli);
    return;
  }

  eds_client_set_on_writable(cli, NULL, 0);
  eds_client_set_on_readable(cli, on_readopen, 0);
}

static const char *get_flagstr(int flags) {
  switch(flags & (O_RDONLY | O_WRONLY | O_RDWR)) {
  case O_RDONLY:
    return "r";
  case O_WRONLY:
    return "w";
  case O_RDWR:
    return "rw";
  default:
    return "?";
  }
}

static void on_readopen(struct eds_client *cli, int fd) {
  char dirpath[STORE_MAXDIRPATH];
  const char *errmsg = "an internal error occurred";
  struct store_cli *ecli = STORE_CLI(cli);
  struct ycl_msg_store_open req = {0};
  size_t pathlen;
  int ret;
  int open_fd;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    eds_client_clear_actions(cli);
    return;
  }

  ret = ycl_msg_parse_store_open(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    errmsg = "open request parse error";
    goto fail;
  }

  /* initialize response fd fields */
  ecli->open_fd = nullfd_get();
  ecli->open_errno = EACCES;

  if (req.path == NULL || *req.path == '\0') {
    LOGERRF(fd, "%s: empty path", STORE_ID(ecli));
    goto sendfd_resp;
  }

  pathlen = strlen(req.path);
  if (!is_valid_path(req.path, pathlen)) {
    if (pathlen > 16) {
        LOGERRF(fd, "%s: invalid path: %.16s...", STORE_ID(ecli), req.path);
    } else {
        LOGERRF(fd, "%s: invalid path: %s", STORE_ID(ecli), req.path);
    }
    goto sendfd_resp;
  }

  snprintf(dirpath, sizeof(dirpath), "%s/%s", ecli->store_path, req.path);
  open_fd = open(dirpath, (int)req.flags, (mode_t)req.mode);
  if (open_fd < 0) {
    ecli->open_errno = errno;
    LOGERRF(fd, "%s: failed to open %s: %s", STORE_ID(ecli), req.path,
        strerror(errno));
  } else {
    const char *flagstr = get_flagstr((int)req.flags);
    LOGINFOF(fd, "%s: opened %s (%s)", STORE_ID(ecli), req.path, flagstr);
    ecli->open_fd = open_fd;
    ecli->open_errno = 0;
  }

sendfd_resp:
  eds_client_set_on_readable(cli, NULL, 0);
  eds_client_set_on_writable(cli, on_sendfd, 0);
  return;

fail:
  write_err_response(cli, fd, errmsg);
}

static void on_post_enter_response(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  ycl_msg_reset(&ecli->msgbuf);
  eds_client_set_on_writable(cli, on_readopen, 0);
}

static void on_enter_response(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  struct ycl_msg_status_resp resp = {0};
  int ret;
  struct eds_transition trans = {
    .flags = EDS_TFLREAD | EDS_TFLWRITE,
    .on_readable = on_post_enter_response,
    .on_writable = NULL,
  };

  resp.okmsg = STORE_ID(ecli);
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    LOGERR(fd, "OK enter response serialization error");
  } else {
    eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
        ycl_msg_nbytes(&ecli->msgbuf), &trans);
  }
}

static void on_readenter(struct eds_client *cli, int fd) {
  const char *errmsg = "an internal error occurred";
  struct store_cli *ecli = STORE_CLI(cli);
  struct ycl_msg_store_enter req = {0};
  int ret;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    errmsg = ycl_strerror(&ecli->ycl);
    goto fail;
  }

  ret = ycl_msg_parse_store_enter(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    errmsg = "enter request parse error";
    goto fail;
  }

  if (req.store_id != NULL) {
    ret = enter_store(ecli, req.store_id, 0);
  } else {
    ret = create_and_enter_store(ecli);
  }

  if (ret < 0) {
    errmsg = "unable to enter store";
    goto fail;
  }

  LOGINFOF(fd, "entered store: %s", STORE_ID(ecli));
  eds_client_set_on_readable(cli, NULL, 0);
  eds_client_set_on_writable(cli, on_enter_response, 0);
  return;
fail:
  write_err_response(cli, fd, errmsg);
}

void store_on_readable(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  int ret;

  ycl_init(&ecli->ycl, fd);
  if (ecli->flags & STOREFL_HASMSGBUF) {
    ycl_msg_reset(&ecli->msgbuf);
  } else {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      LOGERR(fd, "ycl_msg_init failure");
      goto fail;
    }
    ecli->flags |= STOREFL_HASMSGBUF;
  }

  eds_client_set_on_readable(cli, on_readenter, 0);
  return;
fail:
  eds_service_remove_client(cli->svc, cli);
}

void store_on_done(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  /* TODO: Implement */
  (void)ecli;
  LOGINFO(fd, "done");
}

void store_on_finalize(struct eds_client *cli) {
  struct store_cli *ecli = STORE_CLI(cli);
  if (ecli->flags & STOREFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~STOREFL_HASMSGBUF;
  }
}
