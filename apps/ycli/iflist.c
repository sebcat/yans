#include <stdlib.h>
#include <stdio.h>

#include <lib/net/iface.h>
#include <apps/ycli/iflist.h>

int iflist_main(int argc, char *argv[]) {
  int ret;
  struct iface ifs = {0};
  struct iface_entry ent;

  ret = iface_init(&ifs);
  if (ret < 0) {
    fprintf(stderr, "iface_init: %s\n", iface_strerror(&ifs));
    return EXIT_FAILURE;
  }

  while (iface_next(&ifs, &ent) > 0) {
    printf("%s ", ent.name);
  }

  printf("\n");
  iface_cleanup(&ifs);
  return 0;
}
