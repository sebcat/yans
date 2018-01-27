#ifndef LUAYANS_H__
#define LUAYANS_H__

int yans_shell(const char *arg0, int argc, char *argv[]);
int yans_evalfile(const char *filename, const char *arg0, int argc,
    char **argv);

#endif
