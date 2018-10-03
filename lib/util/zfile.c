#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <lib/util/zfile.h>
#include <zlib.h>

/* funopen/fopencookie are nonstandard and a bit quirky. Use with care. */

#if defined(__FreeBSD__)

static int readfn(void *p, char *buf, int len) {
  if (len < 0) {
    return -1;
  }

  return gzread((gzFile)p, buf, len);
}

static int writefn(void *p, const char *data, int len) {
  if (len < 0) {
    return -1;
  }

  return gzwrite((gzFile)p, data, len);
}

static int closefn(void *p) {
  return gzclose((gzFile)p);
}

FILE *zfile_open(const char * restrict path, const char * restrict mode) {
  gzFile z;
  FILE *fp = NULL;

  z = gzopen(path, mode);
  if (z != NULL) {
    fp = funopen(z, readfn, writefn, NULL, closefn);
  }
  return fp;
}

FILE *zfile_fdopen(int fd, const char *mode) {
  gzFile z;
  FILE *fp = NULL;

  z = gzdopen(fd, mode);
  if (z != NULL) {
    fp = funopen(z, readfn, writefn, NULL, closefn);
  }

  return fp;
}

#elif defined(__linux__)
/* glibc has fopencookie, and musl libc master added fopencookie-support in
 * Dec-2017 */

static ssize_t readfn(void *p, char *buf, size_t len) {
  if (len < 0) {
    return -1;
  }

  return gzread((gzFile)p, buf, len);
}

static ssize_t writefn(void *p, const char *data, size_t len) {
  if (len < 0) {
    return -1;
  }

  return gzwrite((gzFile)p, data, len);
}

static int closefn(void *p) {
  return gzclose((gzFile)p);
}

static FILE *gztofp(gzFile z, const char * restrict mode) {
  cookie_io_functions_t funcs = {
    .read = readfn,
    .write = writefn,
    .seek = NULL,
    .close = closefn,
  };

  return fopencookie(z, mode, funcs);
}

FILE *zfile_open(const char * restrict path, const char * restrict mode) {
  gzFile z;
  FILE *fp = NULL;

  z = gzopen(path, mode);
  if (z != NULL) {
    fp = gztofp(z, mode);
  }
  return fp;
}

FILE *zfile_fdopen(int fd, const char *mode) {
  gzFile z;
  FILE *fp = NULL;

  z = gzdopen(fd, mode);
  if (z != NULL) {
    fp = gztofp(z, mode);
  }

  return fp;
}

#else
#error "NYI"
#endif
