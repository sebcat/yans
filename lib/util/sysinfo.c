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
#include <time.h>
#include <string.h>
#include <stdlib.h>

#if defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(__linux__)
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#endif

#include <lib/util/macros.h>
#include <lib/util/sysinfo.h>

#if defined(__FreeBSD__)

void sysinfo_get(struct sysinfo_data *data, const char *root) {
  struct timespec tv;
  int ret;
  struct statfs f;
  int64_t used;
  int64_t availblks;
  int64_t inodes;

  memset(data, 0, sizeof(*data));
  if (root == NULL || data == NULL) {
    return;
  }

  /* get blocksize and inode info of data mount */
  ret = statfs(root, &f);
  if (ret == 0) {
    /* blocksize capacity */
    used = f.f_blocks - f.f_bfree;
    availblks = f.f_bavail + used;
    data->fcap = availblks <= 0 ?
        100.0 : (double)used / (double)availblks * 100.0;
    data->fcap = CLAMP(data->fcap, 0.0, 100.0);

    /* inode capacity */
    inodes = f.f_files;
    used = inodes - f.f_ffree;
    data->icap = inodes <= 0 ?
        100.0 : (double)used / (double)inodes * 100.0;
    data->icap = CLAMP(data->icap, 0.0, 100.0);
  }

  /* get the uptime */
  ret = clock_gettime(CLOCK_UPTIME_FAST, &tv);
  if (ret == 0) {
    data->uptime = tv.tv_sec;
  }

  /* get the load avg */
  getloadavg(data->loadavg, sizeof(data->loadavg)/sizeof(*data->loadavg));
}

#elif defined(__linux__)
void sysinfo_get(struct sysinfo_data *data, const char *root) {
  int ret;
  int i;
  struct statvfs f;
  struct sysinfo si;
  int64_t used;
  int64_t availblks;
  int64_t inodes;

  memset(data, 0, sizeof(*data));
  if (root == NULL || data == NULL) {
    return;
  }

  /* get blocksize and inode info of data mount */
  ret = statvfs(root, &f);
  if (ret == 0) {
    /* blocksize capacity */
    used = f.f_blocks - f.f_bfree;
    availblks = f.f_bavail + used;
    data->fcap = availblks <= 0 ?
        100.0 : (double)used / (double)availblks * 100.0;
    data->fcap = CLAMP(data->fcap, 0.0, 100.0);

    /* inode capacity */
    inodes = f.f_files;
    used = inodes - f.f_ffree;
    data->icap = inodes <= 0 ?
        100.0 : (double)used / (double)inodes * 100.0;
    data->icap = CLAMP(data->icap, 0.0, 100.0);
  }

  /* get the uptime and load avg */
  ret = sysinfo(&si);
  if (ret == 0) {
    data->uptime = si.uptime;
    for (i = 0; i < 3; i++) {
      data->loadavg[i] = si.loads[i] / (double)(1 << SI_LOAD_SHIFT);
    }
  }
}

#endif
