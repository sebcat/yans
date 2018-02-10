#ifndef YANS_ZFILE__
#define YANS_ZFILE__

#include <stdio.h>

FILE *zfile_open(const char * restrict path, const char * restrict mode);
FILE *zfile_fdopen(int fd, const char *mode);

#endif
