
#include "lib/util/hexdump.h"

int hexdump(FILE *fp, void *data, size_t len) {
  size_t i;
  size_t mod;
  const unsigned char *cptr = data;

  for (i = 0; i < len; i++) {
    mod = i & 0x0f;
    if (mod == 0) {
      fprintf(fp, "%8zx    ", i);
    }

    if (mod == 15) {
      fprintf(fp, "%.2x\n", (unsigned int)cptr[i]);
    } else {
      fprintf(fp, "%.2x ", (unsigned int)cptr[i]);
    }
  }

  if (ferror(fp)) {
    return -1;
  }

  return 0;
}
