/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <lib/lua/yans.h>

int main(int argc, char *argv[]) {
  int exitcode = EXIT_FAILURE;

  signal(SIGPIPE, SIG_IGN);

  if (argc == 1) {
    if (isatty(STDIN_FILENO)) {
      exitcode = yans_shell(argv[0], argc-1, argv+1);
    } else {
      exitcode = yans_evalfile(NULL, argv[0], argc-1, argv+1);
    }
  } else if (argc > 1) {
    if (argv[1][0] == '-' && argv[1][1] == '-' && argv[1][2] == '\0') {
      if (isatty(STDIN_FILENO)) {
        exitcode = yans_shell(argv[0], argc-2, argv+2);
      } else {
        exitcode = yans_evalfile(NULL, argv[0], argc-2, argv+2);
      }
    } else {
      exitcode = yans_evalfile(argv[1], argv[1], argc-2, argv+2);
    }
  }

  return exitcode;
}
