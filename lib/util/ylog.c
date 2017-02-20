#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <lib/util/ylog.h>

static struct {
  void (*log_fatal)(const char *str);
  void (*log_error)(const char *str);
  void (*log_info)(const char *str);
  char ident[64];
} ylogger = {
  /* A bit of paranoia. ylogger is static, so the fields should already be
   * zeroed as per e.g., C99 6.7.8 constraint 10 */
  .log_fatal = NULL,
  .log_error = NULL,
  .log_info = NULL,
  .ident = {0},
};

static void syslog_fatal(const char *str) {
  syslog(LOG_CRIT, "%s", str);
}

static void syslog_error(const char *str) {
  syslog(LOG_ERR, "%s", str);
}

static void syslog_info(const char *str) {
  syslog(LOG_INFO, "%s", str);
}


static void stderr_logger(const char *s) {
  char msgbuf[YLOG_MSGSZ];
  char tstamp[64];
  struct tm tm;
  time_t clk;

  clk = time(NULL);
  localtime_r(&clk, &tm);
  strftime(tstamp, sizeof(tstamp), "%b %d %H:%M:%S", &tm);
  snprintf(msgbuf, sizeof(msgbuf), "%s %s[%d]: %s", tstamp, ylogger.ident,
      (int)getpid(), s);
  fprintf(stderr, "%s\n", msgbuf);
  fflush(stderr);
}


void ylog_fatal(int action, const char *fmt, ...) {
  va_list ap;
  char msgbuf[YLOG_MSGSZ];

  if (ylogger.log_fatal != NULL) {
    va_start(ap, fmt);
    vsnprintf(msgbuf, YLOG_MSGSZ, fmt, ap);
    ylogger.log_fatal(msgbuf);
    va_end(ap);
  }

  if (action == YLOG_ACTABRT) {
    abort();
  } else if (action == YLOG_ACTEXIT) {
    exit(1);
  }
}

void ylog_error(const char *fmt, ...) {
  char msgbuf[YLOG_MSGSZ];
  va_list ap;
  if (ylogger.log_error != NULL) {
    va_start(ap, fmt);
    vsnprintf(msgbuf, YLOG_MSGSZ, fmt, ap);
    ylogger.log_error(msgbuf);
    va_end(ap);
  }
}

void ylog_perror(const char *str) {
  ylog_error("%s: %s", str, strerror(errno));
}

void ylog_info(const char *fmt, ...) {
  char msgbuf[YLOG_MSGSZ];
  va_list ap;
  if (ylogger.log_info != NULL) {
    va_start(ap, fmt);
    vsnprintf(msgbuf, YLOG_MSGSZ, fmt, ap);
    ylogger.log_info(msgbuf);
    va_end(ap);
  }
}

void ylog_init(const char *ident, int logger) {
  if (logger == YLOG_SYSLOG) {
    openlog(ylogger.ident, LOG_NDELAY|LOG_PID, LOG_DAEMON);
    ylogger.log_fatal = &syslog_fatal;
    ylogger.log_error = &syslog_error;
    ylogger.log_info = &syslog_info;
  } else if (logger == YLOG_STDERR) {
    ylogger.log_fatal = &stderr_logger;
    ylogger.log_error = &stderr_logger;
    ylogger.log_info = &stderr_logger;
  } else {
    return;
  }

  if (ident != NULL) {
    snprintf(ylogger.ident, sizeof(ylogger.ident), "%s", ident);
  }
}
