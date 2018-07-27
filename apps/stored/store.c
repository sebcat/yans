#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fts.h>
#include <regex.h>

#include <lib/util/ylog.h>
#include <lib/util/prng.h>
#include <lib/util/nullfd.h>
#include <lib/util/sindex.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/stored/store.h>

#if (STORE_IDSZ != SINDEX_IDSZ)
#error "Store ID must be of the same length as Index ID"
#endif

#define STORE_PATH "store"
#define STORE_INDEX "INDEX"

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

static struct prng_ctx prng_;     /* PRNG */
static struct sindex_ctx sindex_; /* Store Index */

static void gen_store_path(char *data, size_t len) {
  if (data == NULL || len == 0) {
    return;
  }

  prng_hex(&prng_, data, len-1);
  data[len-1] = '\0';
}

static int init_index(struct sindex_ctx *ctx) {
  int fd;
  FILE *fp;

  fd = open(STORE_INDEX, O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (fd < 0) {
    return -1;
  }

  fp = fdopen(fd, "a");
  if (!fp) {
    close(fd);
    return -1;
  }

  sindex_init(ctx, fp);
  return 0;
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

  /* init the index or error out */
  ret = init_index(&sindex_);
  if (ret < 0) {
    ylog_error("store: failed to initialize store index: %s", strerror(errno));
    goto fail;
  }

  /* we use the PRNG for directory/file names, so it should be fine seeding
   * it with the current time (famous last words) */
  t = time(NULL);
  if (t == (time_t)-1) {
    ylog_error("store: failed to obtain initial seed (%s)", strerror(errno));
    goto fail;
  }

  seed = (uint32_t)t;
  prng_init(&prng_, seed);
  ylog_info("store: initialized with seed: %u", seed);
  return 0;

fail:
  return -1;
}

void store_fini(struct eds_service *svc) {
  FILE *fp;

  fp = sindex_fp(&sindex_);
  fclose(fp);
}

static void write_err_response(struct eds_client *cli, int fd,
    const char *errmsg) {
  struct store_cli *ecli = STORE_CLI(cli);
  struct ycl_msg_status_resp resp = {{0}};
  int ret;

  LOGERR(fd, errmsg);
  eds_client_clear_actions(cli);
  resp.errmsg.data = errmsg;
  resp.errmsg.len = strlen(errmsg);
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
    size_t id_len, int exclusive) {
  const char *subdir;
  int ret;

  if (!is_valid_store_id(store_id, id_len)) {
    return -1;
  }

  subdir = store_id + STORE_IDSZ - STORE_PREFIXSZ;
  ret = mkdir(subdir, 0700);
  if (ret != 0 && errno != EEXIST) {
    return -1;
  }

  snprintf(ecli->store_path, sizeof(ecli->store_path), "%s/%s",
      subdir, store_id);
  ret = mkdir(ecli->store_path, 0700);
  if (ret != 0 && (exclusive || errno != EEXIST)) {
    return -1;
  }

  return 0;
}

static int put_index(char *store_id) {
  struct sindex_entry ie = {0};

  memcpy(ie.id, store_id, SINDEX_IDSZ);
  ie.indexed = time(NULL);
  return sindex_put(&sindex_, &ie);
}

static int create_and_enter_store(int fd, struct store_cli *ecli) {
  char store_id[STORE_IDSZ+1];
  int i;
  int ret;

  for (i = 0; i < MAXTRIES_GENSTORE; i++) {
    gen_store_path(store_id, sizeof(store_id));
    ret = enter_store(ecli, store_id, STORE_IDSZ, 1);
    if (ret == 0) {
      if (put_index(store_id) < 0) {
        /* indexing is not *that* important, so only log this */
        LOGERRF(fd, "unable to index newly created store %s", store_id);
      }
      break;
    }
  }

  return ret;
}

static void on_readentered(struct eds_client *cli, int fd);

static void on_sendfd(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  int ret;

  ret = ycl_sendfd(&ecli->ycl, ecli->open_fd, ecli->open_errno);
  if (ret == YCL_AGAIN) {
    return;
  }

  if (ecli->open_fd >= 0 && ecli->open_fd != nullfd_get()) {
    close(ecli->open_fd);
  }

  if (ret != YCL_OK) {
    LOGERRF(fd, "%s: err sendfd: %s", STORE_ID(ecli),
        ycl_strerror(&ecli->ycl));
    eds_client_clear_actions(cli);
    return;
  }

  eds_client_set_on_writable(cli, NULL, 0);
  eds_client_set_on_readable(cli, on_readentered, 0);
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

static void handle_store_open(struct eds_client *cli, int fd, 
    struct ycl_msg_store_entered_req *req) {
  int open_fd;
  char dirpath[STORE_MAXDIRPATH];
  struct store_cli *ecli = STORE_CLI(cli);
  /* initialize response fd fields */
  ecli->open_fd = nullfd_get();
  ecli->open_errno = EACCES;

  if (req->open_path.data == NULL || *req->open_path.data == '\0') {
    LOGERRF(fd, "%s: empty path", STORE_ID(ecli));
    goto sendfd_resp;
  }

  if (!is_valid_path(req->open_path.data, req->open_path.len)) {
    if (req->open_path.len > 16) {
        LOGERRF(fd, "%s: invalid path: %.16s...", STORE_ID(ecli),
            req->open_path.data);
    } else {
        LOGERRF(fd, "%s: invalid path: %s", STORE_ID(ecli),
            req->open_path.data);
    }
    goto sendfd_resp;
  }

  snprintf(dirpath, sizeof(dirpath), "%s/%s", ecli->store_path,
      req->open_path.data);
  if ((int)req->open_flags & O_CREAT) {
    open_fd = open(dirpath, (int)req->open_flags, 0600);
  } else {
    open_fd = open(dirpath, (int)req->open_flags);
  }
  if (open_fd < 0) {
    ecli->open_errno = errno;
    LOGERRF(fd, "%s: failed to open %s: %s", STORE_ID(ecli),
        req->open_path.data, strerror(errno));
  } else {
    const char *flagstr = get_flagstr((int)req->open_flags);
    LOGINFOF(fd, "%s: opened %s (%s)", STORE_ID(ecli), req->open_path.data,
        flagstr);
    ecli->open_fd = open_fd;
    ecli->open_errno = 0;
  }

sendfd_resp:
  eds_client_set_on_readable(cli, NULL, 0);
  eds_client_set_on_writable(cli, on_sendfd, 0);
  return;
}

static void handle_store_rename(struct eds_client *cli, int fd, 
    struct ycl_msg_store_entered_req *req) {
  /* TODO: Implement */
}

static void on_readentered(struct eds_client *cli, int fd) {
  const char *errmsg = "an internal error occurred";
  struct store_cli *ecli = STORE_CLI(cli);
  struct ycl_msg_store_entered_req req = {{0}};
  int ret;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    eds_client_clear_actions(cli);
    return;
  }

  ret = ycl_msg_parse_store_entered_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    errmsg = "open request parse error";
    goto fail;
  }

  if (req.action.len == 0) {
    errmsg = "missing 'action' field in request";
    goto fail;
  }

  if (strcmp(req.action.data, "open") == 0) {
    handle_store_open(cli, fd, &req);
  } else if (strcmp(req.action.data, "rename") == 0) {
    handle_store_rename(cli, fd, &req);
  } else {
    errmsg = "unknown 'action' field in request";
    goto fail;
  }

  return;
fail:
  write_err_response(cli, fd, errmsg);
}

static void on_post_enter_response(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  ycl_msg_reset(&ecli->msgbuf);
  eds_client_set_on_writable(cli, on_readentered, 0);
}

static void on_enter_response(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  struct ycl_msg_status_resp resp = {{0}};
  int ret;
  struct eds_transition trans = {
    .flags = EDS_TFLREAD | EDS_TFLWRITE,
    .on_readable = on_post_enter_response,
    .on_writable = NULL,
  };

  resp.okmsg.data = STORE_ID(ecli);
  resp.okmsg.len = strlen(resp.okmsg.data);
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    LOGERR(fd, "OK enter response serialization error");
  } else {
    eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
        ycl_msg_nbytes(&ecli->msgbuf), &trans);
  }
}

static void list_store_content(buf_t *buf, const char *store,
    const regex_t *must_match) {
  FTS *fts;
  FTSENT *ent;
  char storepath[STORE_MAXDIRPATH];
  const char *subdir;
  char *paths[2];
  char numbuf[24];

  /* setup the path to the store */
  subdir = store + STORE_IDSZ - STORE_PREFIXSZ;
  snprintf(storepath, sizeof(storepath), "%s/%s", subdir, store);
  paths[0] = storepath;
  paths[1] = NULL;

  buf_clear(buf);
  fts = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
  if (fts == NULL) {
    return;
  }

  while ((ent = fts_read(fts))) {
    if (ent->fts_info == FTS_D && ent->fts_level == 2) {
      fts_set(fts, ent, FTS_SKIP);
    } else if (ent->fts_info == FTS_F) {
      if (must_match && regexec(must_match, ent->fts_name, 0, NULL, 0) != 0) {
        continue;
      }

      /* fmt: name\0size\0 */
      snprintf(numbuf, sizeof(numbuf), "%lu",
          (unsigned long)ent->fts_statp->st_size);
      buf_adata(buf, ent->fts_name, strlen(ent->fts_name) + 1);
      buf_adata(buf, numbuf, strlen(numbuf) + 1);
    }
  }

  fts_close(fts);
}

static void list_stores(buf_t *buf, const regex_t *must_match) {
  FTS *fts;
  FTSENT *ent;
  char *paths[] = {".", NULL};

  buf_clear(buf);
  fts = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
  if (fts == NULL) {
    return;
  }

  while ((ent = fts_read(fts))) {
    if (ent->fts_info == FTS_D && ent->fts_level == 2) {
      fts_set(fts, ent, FTS_SKIP);
      if (must_match && regexec(must_match, ent->fts_name, 0, NULL, 0) != 0) {
        continue;
      }

      buf_adata(buf, ent->fts_name, strlen(ent->fts_name) + 1);
    }
  }

  fts_close(fts);
}

static void on_index_response(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  int ret;

  ret = ycl_sendfd(&ecli->ycl, ecli->open_fd, ecli->open_errno);
  if (ret == YCL_AGAIN) {
    return;
  }

  if (ecli->open_fd >= 0 && ecli->open_fd != nullfd_get()) {
    close(ecli->open_fd);
  }

  if (ret != YCL_OK) {
    LOGERRF(fd, "%s: err sendfd: %s", STORE_ID(ecli),
        ycl_strerror(&ecli->ycl));
  }

  if (ecli->open_fd < 0) {
    LOGERRF(fd, "index: failed to open fd: %s", strerror(ecli->open_errno));
  }

  eds_client_clear_actions(cli);
}

static void on_sent_store_list(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  buf_cleanup(&ecli->buf);
  eds_client_clear_actions(cli);
}

static void handle_store_list(struct eds_client *cli, int fd,
    struct ycl_msg_store_req *req) {
  int ret;
  struct store_cli *ecli = STORE_CLI(cli);
  const char *errmsg = NULL;
  regex_t re;
  regex_t *rep = NULL;
  struct ycl_msg_store_list listmsg = {{0}};
  struct eds_transition after_send = {
    .flags       = EDS_TFLREAD | EDS_TFLWRITE,
    .on_readable = on_sent_store_list,
    .on_writable = on_sent_store_list,
  };

  /* validate store_id, if any */
  if (req->store_id.len > 0 &&
      !is_valid_store_id(req->store_id.data, req->store_id.len)) {
    errmsg = "invalid store ID";
    goto done;
  }

  /* check the must-match ERE, if any */
  if (req->list_must_match.len > 0) {
    ret = regcomp(&re, req->list_must_match.data, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
      errmsg = "failed to compile must-match ERE";
      goto done;
    }
    rep = &re;
  }

  /* set up the store list */
  buf_init(&ecli->buf, 8192);
  if (req->store_id.len > 0) {
    list_store_content(&ecli->buf, req->store_id.data, rep);
  } else {
    list_stores(&ecli->buf, rep);
  }

  /* clean-up the ERE, if any */
  if (rep != NULL) {
    regfree(rep);
  }

done:
  if (errmsg) {
    LOGERRF(fd, "store list failure: %s", errmsg);
  } else if (req->store_id.len > 0) {
    LOGINFOF(fd, "%s: listed store", req->store_id.data);
  } else {
    LOGINFO(fd, "listed stores");
  }

  /* setup the response */
  listmsg.errmsg.data = errmsg;
  listmsg.errmsg.len = errmsg ? strlen(errmsg) : 0;
  listmsg.entries.data = ecli->buf.data;
  listmsg.entries.len = ecli->buf.len;
  ycl_msg_create_store_list(&ecli->msgbuf, &listmsg);
  eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
      ycl_msg_nbytes(&ecli->msgbuf), &after_send);
}

static void on_readreq(struct eds_client *cli, int fd) {
  const char *errmsg = "an internal error occurred";
  struct store_cli *ecli = STORE_CLI(cli);
  struct ycl_msg_store_req req = {{0}};
  int ret;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    eds_client_clear_actions(cli);
    return;
  }

  ret = ycl_msg_parse_store_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    errmsg = "enter request parse error";
    goto fail;
  }

  if (req.action.len == 0) {
    errmsg = "missing store action";
    goto fail;
  } else if (strcmp(req.action.data, "enter") == 0) {
    if (req.store_id.data != NULL) {
      ret = enter_store(ecli, req.store_id.data, req.store_id.len, 0);
    } else {
      ret = create_and_enter_store(fd, ecli);
    }

    if (ret < 0) {
      errmsg = "unable to enter store";
      goto fail;
    }

    LOGINFOF(fd, "%s: entered store", STORE_ID(ecli));
    eds_client_set_on_readable(cli, NULL, 0);
    eds_client_set_on_writable(cli, on_enter_response, 0);
  } else if (strcmp(req.action.data, "index") == 0) {
    /* the sole purpose of 'index' is to pass an fd of the index file, opened
     * as read-only  */
    ecli->open_fd = open(STORE_INDEX, O_RDONLY);
    ecli->open_errno = ecli->open_fd < 0 ? errno : 0;
    eds_client_set_on_readable(cli, NULL, 0);
    eds_client_set_on_writable(cli, on_index_response, 0);
  } else if (strcmp(req.action.data, "list") == 0) {
    handle_store_list(cli, fd, &req);
  } else {
    errmsg = "invalid store action";
    goto fail;
  }

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

  eds_client_set_on_readable(cli, on_readreq, 0);
  return;
fail:
  eds_service_remove_client(cli->svc, cli);
}

void store_on_finalize(struct eds_client *cli) {
  struct store_cli *ecli = STORE_CLI(cli);
  if (ecli->flags & STOREFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~STOREFL_HASMSGBUF;
  }
}
