#ifndef OS_H_
#define OS_H_

#include <unistd.h>

#define OS_OK   0
#define OS_ERR -1

#define OS_ERRBUFSZ    1024

typedef struct os_t {
  /* these fields are internal and should not be used directly. They are
     defined in the header to allow os_t variables to be automatic */

  char errbuf[OS_ERRBUFSZ];
} os_t;


const char *os_strerror(os_t *os);
int os_mkdirp(os_t *os, const char *path, mode_t mode, uid_t owner,
    gid_t group);
int os_remove_all(os_t *os, const char *path);

struct os_chrootd_opts {
  const char *name; /* name of chroot - must be a valid file name */
  const char *path; /* absolute path to chroot directory, must exist */
  uid_t uid;        /* uid of chroot process */
  gid_t gid;        /* gid of chroot process */
  size_t nagroups;  /* number of additional groups of chroot process */
  gid_t *agroups;   /* additional groups of chroot process (see setgroups) */
};

/* os_chrootd --
 *   chroots to a directory. Creates a PID-file in the chroot, and a .dump
 *   file for data written to stdout/stderr. Changes real and effecive
 *   uid/gid, as well as additional groups. Daemonizes the process.
 *
 *   The chroot must exist and have the neccessary device files. Care must be
 *   taken to not map any device files that can allow chroot escape.
 *
 *   If a PID-file already exists the call fails; no testing for stale PID
 *   files are performed. The PID file is created with O_EXCL but no locks are
 *   used.
 *
 *   No other file descriptors than STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
 *   are changed; the daemon will inherit opened file descriptors unless they
 *   have the FD_CLOEXEC flag set.
 *
 *   Must be called as root */
int os_chrootd(os_t *os, struct os_chrootd_opts *opts);


#endif

