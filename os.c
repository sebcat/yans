#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <fts.h>

#include "os.h"

/* must be set at the beginning of every os_* function that can fail */
#define CLEARERRBUF(os) ((os)->errbuf[0] = '\0')

#if !defined(UID_ROOT)
#define UID_ROOT 0
#endif

static void os_seterr(os_t *os,const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void os_setpatherr(os_t *os, const char *fname, const char *path,
    const char *fmt, ...) __attribute__((format(printf, 4, 5)));

static void os_seterr(os_t *os,const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(os->errbuf, OS_ERRBUFSZ, fmt, ap);
  va_end(ap);
}

static void os_setpatherr(os_t *os, const char *fname, const char *path,
    const char *fmt, ...) {
  va_list ap;
  int len;

  va_start(ap, fmt);
  len = snprintf(os->errbuf, OS_ERRBUFSZ, "%s %s: ", fname, path);
  vsnprintf(os->errbuf+len, OS_ERRBUFSZ-len, fmt, ap);
  va_end(ap);
}

const char *os_strerror(os_t *os) {
  return os->errbuf;
}

int os_mkdirp(os_t *os, const char *path, mode_t mode, uid_t owner,
    gid_t group) {
  size_t pathlen = strlen(path);
  CLEARERRBUF(os);
  {
    char buf[pathlen+1], *curr;

    if (path == NULL || pathlen == 0) {
      return OS_OK;
    }

    strncpy(buf, path, pathlen+1);

    /* trim trailing slashes */
    for(curr = buf+pathlen-1; curr > buf && *curr == '/'; curr--) {
      *curr = '\0';
    }

    if (curr == buf) {
      return OS_OK;
    }

    for(curr = buf+1; *curr != '\0'; curr++) {
      if (*curr == '/') {
        *curr = '\0';
        if (mkdir(buf, mode) == 0) {
          if (chown(buf, owner, group) != 0) {
            os_setpatherr(os, "chown", buf, "%s", strerror(errno));
            return OS_ERR;
          }
        } else if (errno != EEXIST && errno != EISDIR) {
          os_setpatherr(os, "mkdir", buf, "%s", strerror(errno));
          return OS_ERR;
        }
        *curr = '/';
        while (curr[1] == '/') {
          curr++;
        }
      }
    }

    if (mkdir(buf, mode) == 0) {
      if (chown(buf, owner, group) != 0) {
        os_setpatherr(os, "chown", buf, "%s", strerror(errno));
        return OS_ERR;
      }
    } else if (errno != EEXIST && errno != EISDIR) {
      os_setpatherr(os, "mkdir", buf, "%s", strerror(errno));
      return OS_ERR;
    }
  }
  return OS_OK;
}

int os_remove_all(os_t *os, const char *path) {
  char * const paths[] = {(char*)path, NULL};
  FTSENT *ent;
  FTS *fts = NULL;

  CLEARERRBUF(os);
  if ((fts = fts_open(paths, FTS_PHYSICAL, NULL)) == NULL) {
    os_setpatherr(os, "fts_open", path, "%s", strerror(errno));
    return OS_ERR;
  }

  while ((ent = fts_read(fts)) != NULL) {
    if (ent->fts_info == FTS_DP) {
      if (rmdir(ent->fts_path) != 0) {
        os_setpatherr(os, "rmdir", ent->fts_path, "%s", strerror(errno));
        goto fail;
      }
    } else if (ent->fts_info == FTS_F ||
        ent->fts_info == FTS_SL ||
        ent->fts_info ==FTS_SLNONE) {
      if (unlink(ent->fts_name) != 0) {
        os_setpatherr(os, "unlink", ent->fts_path, "%s", strerror(errno));
        goto fail;
      }
    }
  }

  if (fts_close(fts) != 0) {
    os_setpatherr(os, "unlink", path, "%s", strerror(errno));
    return OS_ERR;
  }

  return OS_OK;

fail:
  if (fts != NULL) {
    fts_close(fts);
  }

  return OS_ERR;
}

int os_chrootd(os_t *os, struct os_chrootd_opts *opts) {
  uid_t uid;
  pid_t pid;
  int pidfilefd = -1, dumpfilefd = -1, ret, len;
  char buf[1024];

  CLEARERRBUF(os);

  /* validate opts and current euid */
  if (opts->path == NULL || *opts->path == '\0') {
    os_seterr(os, "os_chrootd: path not set");
    goto fail;
  } else if (opts->name == NULL || *opts->name == '\0') {
    os_seterr(os, "os_chrootd: name not set");
    goto fail;
  } else if (opts->path[0] != '/') {
    os_seterr(os, "os_chrootd: path must be absolute");
    goto fail;
  } else if ((uid = geteuid()) != UID_ROOT) {
    os_seterr(os, "os_chrootd: invoked as non-root user (%d)", uid);
    goto fail;
  }

  /* chdir and chroot */
  if (chdir(opts->path) != 0) {
    os_setpatherr(os, "chdir", opts->path, "%s", strerror(errno));
    goto fail;
  } else if (chroot(opts->path) != 0) {
    os_setpatherr(os, "chroot", opts->path, "%s", strerror(errno));
    goto fail;
  }

  /* create the PID file with correct UID/GID, or fail if it exists */
  snprintf(buf, sizeof(buf), "%.*s.pid", (int)sizeof(buf)-8, opts->name);
  if ((pidfilefd = open(buf, O_WRONLY|O_CREAT|O_EXCL, 0644)) < 0) {
    os_seterr(os, "os_chrootd: PID file already exists");
    goto fail;
  } else if (fchown(pidfilefd, opts->uid, opts->gid) != 0) {
    os_seterr(os, "os_chrootd: pidfile chown: %s", strerror(errno));
    goto fail;
  }

  /* create the dumpfile with correct UID/GID, truncate if it exists */
  snprintf(buf, sizeof(buf), "%.*s.dump", (int)sizeof(buf)-8, opts->name);
  if ((dumpfilefd = open(buf, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
    os_seterr(os, "os_chrootd: unable to create dumpfile: %s",
        strerror(errno));
    goto fail;
  } else if (fchown(dumpfilefd, opts->uid, opts->gid) != 0) {
    os_seterr(os, "os_chrootd: dumpfile chown: %s", strerror(errno));
    goto fail;
  }

  /* change additional groups */
  if (opts->nagroups == 0) {
    gid_t gid = opts->gid;
    if (setgroups(1, &gid) != 0) {
      os_seterr(os, "os_chrootd: setgroups failure: %s", strerror(errno));
      goto fail;
    }
  } else if (setgroups(opts->nagroups, opts->agroups) != 0) {
    os_seterr(os, "os_chrootd: setgroups failure: %s", strerror(errno));
    goto fail;
  }

  /* change gid and pid */
  if (setgid(opts->gid) != 0) {
    os_seterr(os, "os_chrootd: setgid failure: %s", strerror(errno));
    goto fail;
  } else if (setuid(opts->uid) != 0) {
    os_seterr(os, "os_chrootd: setuid failure: %s", strerror(errno));
    goto fail;
  }

  /* daemonize - first fork */
  if ((pid = fork()) < 0) {
    os_seterr(os, "os_chrootd: fork failure: %s", strerror(errno));
    goto fail;
  } else if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* daemonize - setsid and second fork */
  setsid();
  umask(0);
  if ((pid = fork()) < 0) {
    os_seterr(os, "os_chrootd: second fork failure: %s", strerror(errno));
    goto fail;
  } else if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* replace standard file descriptors with the dumpfile and write pid to
   * pidfile */
  dup2(dumpfilefd, STDIN_FILENO);
  dup2(dumpfilefd, STDOUT_FILENO);
  dup2(dumpfilefd, STDERR_FILENO);
  close(dumpfilefd);
  len = snprintf(buf, sizeof(buf), "%d\n", getpid());
  do {
    ret = write(pidfilefd, buf, (size_t)len);
  } while (ret < 0 && errno == EINTR);
  close(pidfilefd);

  return OS_OK;

fail:
  if (pidfilefd >= 0) {
    close(pidfilefd);
  }

  if (dumpfilefd >= 0) {
    close(dumpfilefd);
  }

  return OS_ERR;
}
