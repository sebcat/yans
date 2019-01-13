#ifndef YANS_SYSINFO_H__
#define YANS_SYSINFO_H__

struct sysinfo_data {
  double fcap;       /* filesystem capacity in range [0.0-100.0] */
  double icap;       /* filesystem inode capacity in range [0.0-100.0] */
  time_t uptime;     /* uptime, in seconds */
  double loadavg[3]; /* 1,5,15 minutes load average */
};

void sysinfo_get(struct sysinfo_data *data, const char *root);

#endif
