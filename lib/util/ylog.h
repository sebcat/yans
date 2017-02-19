#ifndef YLOG_H_
#define YLOG_H_

/* ylog logger types for ylog_init */
#define YLOG_SYSLOG 1
#define YLOG_STDERR 2

/* Having a 1K message size is partly due to being conservative and being able
 * to use syslog efficiently, and partly because of idealistic reasons. It is
 * my belief that log messages should be short and to the point. Having a
 * smallish message size hopefully encourages that. I am tired of looking at
 * deep stack traces that doesn't convey any actionable information at this
 * point in my life. If you need a stack trace, you're probably better off
 * with a coredump anyway. And while we're at it: no news is good news.
 * End rant. */
#define YLOG_MSGSZ 1024

/* YLOG_ACT* - ylog_fatal actions */
#define YLOG_ACTABRT 1
#define YLOG_ACTEXIT 2

void ylog_init(const char *ident, int logger);
void ylog_fatal(int action, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));
void ylog_error(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void ylog_info(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

#endif
