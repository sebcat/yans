/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <fts.h>
#include <pwd.h>
#include <grp.h>

#include <lib/util/os.h>

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

static void os_perror(os_t *os, const char *s) {
  os_seterr(os, "%s: %s", s, strerror(errno));
}

const char *os_strerror(os_t *os) {
  return os->errbuf;
}

int os_mkdirp(os_t *os, const char *path, mode_t mode, uid_t owner,
    gid_t group) {
  char buf[2048];
  char *curr;
  size_t pathlen;

  CLEARERRBUF(os);

  if (path == NULL || *path == '\0') {
    return OS_OK; /* creating nothing successfully */
  }

  pathlen = strlen(path);
  if (pathlen >= sizeof(buf)) {
    os_seterr(os, "%s: path too long", __func__);
    return OS_ERR;
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

int os_daemon_remove_pidfile(os_t *os, struct os_daemon_opts *opts) {
  char buf[1024];
  int ret;

  CLEARERRBUF(os);
  /* path assumes os_daemonize has been called and set the CWD */
  snprintf(buf, sizeof(buf), "%.*s.pid", (int)sizeof(buf)-8, opts->name);
  ret = unlink(buf);
  if (ret < 0) {
    os_setpatherr(os, "unlink", buf, "%s", strerror(errno));
    goto fail;
  }

  return OS_OK;
fail:
  return OS_ERR;
}

int os_daemonize(os_t *os, struct os_daemon_opts *opts) {
  uid_t uid;
  pid_t pid;
  int pidfilefd = -1;
  int dumpfilefd = -1;
  int ret;
  int len;
  char buf[1024];

  CLEARERRBUF(os);

  /* validate opts and current euid */
  if (opts->path == NULL || *opts->path == '\0') {
    os_seterr(os, "os_daemonize: path not set");
    goto fail;
  } else if (opts->name == NULL || *opts->name == '\0') {
    os_seterr(os, "os_daemonize: name not set");
    goto fail;
  } else if (opts->path[0] != '/') {
    os_seterr(os, "os_daemonize: path must be absolute");
    goto fail;
  } else if ((uid = geteuid()) != UID_ROOT) {
    os_seterr(os, "os_daemonize: invoked as non-root user (%d)", uid);
    goto fail;
  }

  /* chdir */
  if (chdir(opts->path) != 0) {
    os_setpatherr(os, "chdir", opts->path, "%s", strerror(errno));
    goto fail;
  }

  /* chroot unless no chroot is wanted */
  if (!(opts->flags & DAEMONOPT_NOCHROOT)) {
    if (chroot(opts->path) != 0) {
      os_setpatherr(os, "chroot", opts->path, "%s", strerror(errno));
      goto fail;
    }
  }

  /* create the PID file with correct UID/GID, or fail if it exists */
  snprintf(buf, sizeof(buf), "%.*s.pid", (int)sizeof(buf)-8, opts->name);
  if ((pidfilefd = open(buf, O_WRONLY|O_CREAT|O_EXCL, 0644)) < 0) {
    os_perror(os, "os_daemonize: pidfile open");
    goto fail;
  } else if (fchown(pidfilefd, opts->uid, opts->gid) != 0) {
    os_perror(os, "os_daemonize: pidfile chown");
    goto fail;
  }

  /* create the dumpfile with correct UID/GID, truncate if it exists */
  snprintf(buf, sizeof(buf), "%.*s.dump", (int)sizeof(buf)-8, opts->name);
  if ((dumpfilefd = open(buf, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
    os_perror(os, "os_daemonize: dumpfile open");
    goto fail;
  } else if (fchown(dumpfilefd, opts->uid, opts->gid) != 0) {
    os_perror(os, "os_daemonize: dumpfile chown");
    goto fail;
  }

  /* change additional groups */
  if (opts->nagroups == 0) {
    gid_t gid = opts->gid;
    if (setgroups(1, &gid) != 0) {
      os_perror(os, "os_daemonize: setgroups");
      goto fail;
    }
  } else if (setgroups(opts->nagroups, opts->agroups) != 0) {
    os_perror(os, "os_daemonize: setgroups");
    goto fail;
  }

  /* change gid and pid */
  if (setgid(opts->gid) != 0) {
    os_perror(os, "os_daemonize: setgid");
    goto fail;
  } else if (setuid(opts->uid) != 0) {
    os_perror(os, "os_daemonize: setuid");
    goto fail;
  }

  /* daemonize - first fork */
  if ((pid = fork()) < 0) {
    os_perror(os, "os_daemonize: fork");
    goto fail;
  } else if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* daemonize - setsid and second fork */
  setsid();
  umask(0);
  if ((pid = fork()) < 0) {
    os_perror(os, "os_daemonize: fork2");
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

/* this may be too small, in which case it's possible to retry getpwnam_r,
 * getgrnam_r with a larger buffer size, but we keep it simple for now and
 * report the error instead */
#define IDBUFSZ (32*1024)

int os_getuid(os_t *os, const char *user, uid_t *uid) {
  char *buf = NULL;
  struct passwd pwd, *res = NULL;

  CLEARERRBUF(os);
  if ((buf = malloc(IDBUFSZ)) == NULL) {
    os_perror(os, "os_getuid: malloc");
    goto fail;
  }

  if (getpwnam_r(user, &pwd, buf, IDBUFSZ, &res) != 0) {
    os_perror(os, "os_getuid: getpwnam_r");
    goto fail;
  } else if (res == NULL) {
    os_seterr(os, "os_getuid: no such user (%s)", user);
    goto fail;
  }

  *uid = res->pw_uid;
  free(buf);
  return OS_OK;

fail:
  if (buf != NULL) {
    free(buf);
  }

  return OS_ERR;
}

int os_getgid(os_t *os, const char *group, gid_t *gid) {
  char *buf = NULL;
  struct group grp, *res = NULL;

  CLEARERRBUF(os);
  if ((buf = malloc(IDBUFSZ)) == NULL) {
    os_perror(os, "os_getpid: malloc");
    goto fail;
  }

  if (getgrnam_r(group, &grp, buf, IDBUFSZ, &res) != 0) {
    os_perror(os, "os_getgid: getgrnam_r");
    goto fail;
  } else if (res == NULL) {
    os_seterr(os, "os_getgid: no such group (%s)", group);
    goto fail;
  }

  *gid = res->gr_gid;
  free(buf);
  return OS_OK;

fail:
  if (buf != NULL) {
    free(buf);
  }

  return OS_ERR;
}

char *os_cleanpath(char *path) {
  size_t r;       /* read offset */
  size_t w;       /* write offset */
  size_t leftlim; /* left-most limit for ..'s */
  int absolute;   /* path begins with / (1) or not (0) */

  if (path[0] == '/') {
    absolute = 1;
    r = 1;
    w = 1;
    leftlim = 1;
  } else {
    absolute = 0;
    r = 0;
    w = 0;
    leftlim = 0;
  }

  while (path[r] != '\0') {
    if (path[r] == '/') {
      r++;
    } else if (path[r] == '.' && (path[r + 1] == '\0' || path[r + 1] == '/')) {
      r++;
    } else if (path[r] == '.' && path[r + 1] == '.' &&
        (path[r + 2] == '\0' || path[r + 2] == '/')) {
      r += 2;
      if (w > leftlim) {
        do {
          w--;
        } while (w > leftlim && path[w] != '/');
      } else if (!absolute) {
        if (w > 0) {
          path[w++] = '/';
        }
        path[w++] = '.';
        path[w++] = '.';
        leftlim = w;
      }
    } else {
      if ((absolute && w != 1) || (!absolute && w > 0)) {
        path[w++] = '/';
      }
      while (path[r] != '\0' && path[r] != '/') {
        path[w++] = path[r++];
      }
    }
  }

  path[w] = '\0';
  return path;
}

int os_isdir(const char *path) {
  struct stat st;

  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return 1;
  } else {
    return 0;
  }
}

int os_isfile(const char *path) {
  struct stat st;

  if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
    return 1;
  } else {
    return 0;
  }
}

int os_isexec(const char *path) {
  return access(path, X_OK) == 0 ? 1 : 0;
}

int os_fdisfile(int fd) {
  struct stat st;

  if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
    return 1;
  } else {
    return 0;
  }
}

/* Covert fopen(3) style modes to open(2) flags. Returns -1 on invalid
 * modestr, 0 on success. */
int os_mode2flags(const char *modestr, int *outflags) {
  char ch;
  int oflags;
  int mods;

  switch (*modestr++) {
  case 'a':
    oflags = O_WRONLY;
    mods = O_CREAT | O_APPEND;
    break;
  case 'r':
    oflags = O_RDONLY;
    mods = 0;
    break;
  case 'w':
    oflags = O_WRONLY;
    mods = O_CREAT | O_TRUNC;
    break;
  default:
    return -1;
  }

  while ((ch = *modestr++) != '\0') {
    switch(ch) {
    case '+':
      oflags = O_RDWR;
      break;
    case 'b':
      break;
    case 'e':
      mods |= O_CLOEXEC;
      break;
    case 'x':
      mods |= O_EXCL;
      break;
    default:
      return -1;
    }
  }

  if (oflags == O_RDONLY && (mods & O_EXCL)) {
    return -1;
  }

  *outflags = oflags | mods;
  return 0;
}
