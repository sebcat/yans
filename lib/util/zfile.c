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

#else /* defined(__FreeBSD__) */
/* glibc has fopencookie, and musl libc master added fopencookie-support in
 * Dec-2017 */
#error "NYI"
#endif
