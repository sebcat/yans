#include <time.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <string.h>
#include <stdlib.h>

#include <lib/util/macros.h>
#include <lib/util/sysinfo.h>

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
