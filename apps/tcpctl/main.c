/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <drivers/freebsd/tcpsrc/tcpsrc.h>
#include <lib/net/ip.h>

#define DEFAULT_TCPCTL_PATH "/dev/tcpctl"

static void disallow_blocks(const char *blkstr, struct ip_blocks *blks,
    int fd) {
  size_t i;
  struct tcpsrc_range range;
  int ret;

  for (i = 0; i < blks->nblocks; i++)  {
    /* XXX: This assumes tcpsrc_range and ip_block_t share the same
     *      layout, which it does at the time of writing but may or may not
     *      in the future */
    memcpy(&range.first, &blks->blocks[i].first, sizeof(range.first));
    memcpy(&range.last, &blks->blocks[i].last, sizeof(range.last));
    ret = ioctl(fd, TCPSRC_DISALLOW_RANGE, &range);
    if (ret < 0) {
      fprintf(stderr, "warn: %s[%zu]: %s\n", blkstr, i, strerror(errno));
    }
  }
}

static void usage(const char *argv0) {
  fprintf(stderr,
      "usage: %s disallow [-d /dev/path] [range0] [range1] ... [rangeN]\n",
      argv0);
  exit(EXIT_FAILURE);
}

static int disallow_main(int argc, char **argv) {
  int ch;
  const char *path = DEFAULT_TCPCTL_PATH;
  int i;
  int fd;
  struct ip_blocks blks;
  int ret;

  while ((ch = getopt(argc - 1, argv + 1, "d:")) != -1) {
    switch(ch) {
    case 'd':
      path = optarg;
      break;
    default:
      usage(argv[0]);
    }
  }

  argc -= optind+1;
  argv += optind+1;

  fd = open(path, O_WRONLY);
  if (fd < 0) {
    perror(path);
    return EXIT_FAILURE;
  }

  for (i = 0; i < argc; i++) {
    ret = ip_blocks_init(&blks, argv[i], NULL);
    if (ret < 0) {
      fprintf(stderr, "warn: failed to parse block (%s)\n", argv[i]);
      continue;
    }

    disallow_blocks(argv[i], &blks, fd);
    ip_blocks_cleanup(&blks);
  }

  close(fd);

  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (argc < 2 || strcmp(argv[1], "disallow") != 0) {
    usage(argv[0]);
  }

  return disallow_main(argc, argv);
}

