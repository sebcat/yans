#ifndef YANS_CSV_H__
#define YANS_CSV_H__

/* RFC4180 */

#include <lib/util/buf.h>

int csv_encode(buf_t *dst, const char **cols, size_t ncols);

#endif
