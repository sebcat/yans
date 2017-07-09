#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <apps/ycli/pcapcli.h>

int main(int argc, char *argv[]) {
  int i;
  static const struct {
    char *name;
    int (*cb)(int argc, char *argv[]);
  } cmds[] = {
    {"pcap", pcapcli_main},
    {0},
  };

  if (argc < 2 || argv[1] == NULL) {
    fprintf(stderr, "usage: %s [subcmd]\n  subcmds: ",
        argv[0] != NULL ? argv[0] : "ycli");
    for (i = 0; cmds[i].name != NULL; i++) {
      fprintf(stderr, "%s ", cmds[i].name);
    }
    fprintf(stderr, "\n");
    return EXIT_FAILURE;
  }

  for (i = 0; cmds[i].name != NULL; i++) {
    if (strcmp(argv[1], cmds[i].name) == 0) {
      return cmds[i].cb(argc-1, argv+1);
    }
  }

  fprintf(stderr, "invalid subcmd\n");
  return EXIT_FAILURE;
}
