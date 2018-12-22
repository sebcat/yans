#ifndef TCPSRC_DRIVER_H__
#define TCPSRC_DRIVER_H__

#include <netinet/in.h>
#include <sys/ioccom.h>

#define TCPSRCF_BLOCKING (1 << 0) /* */

struct tcpsrc_conn {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} u;
};

#define TCPSRC_MAGIC '8'
#define TCPSRC_CONNECT  _IOW(TCPSRC_MAGIC, 0, struct tcpsrc_conn)
/* TODO: per-fd allow,disallow ranges (TCPSRC_ALLOW, TCPSRC_DISALLOW) */

#endif
